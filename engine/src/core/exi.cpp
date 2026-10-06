// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wp/exi.h"

#include <array>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>

#include "wp/memory.h"

namespace wp::exi {

namespace {

constexpr uint32_t kBase = 0xCD006800;
constexpr uint32_t kChannelStride = 0x14;
constexpr uint32_t kChannels = 3;
constexpr uint32_t kStatus = 0x00;
constexpr uint32_t kDmaAddress = 0x04;
constexpr uint32_t kDmaLength = 0x08;
constexpr uint32_t kControl = 0x0C;
constexpr uint32_t kData = 0x10;

constexpr uint32_t kExiIntMask = 1u << 0;
constexpr uint32_t kExiInt = 1u << 1;
constexpr uint32_t kTcIntMask = 1u << 2;
constexpr uint32_t kTcInt = 1u << 3;
constexpr uint32_t kClockMask = 7u << 4;
constexpr uint32_t kChipSelectShift = 7;
constexpr uint32_t kChipSelectMask = 7u << kChipSelectShift;
constexpr uint32_t kExtIntMask = 1u << 10;
constexpr uint32_t kExtInt = 1u << 11;
constexpr uint32_t kExt = 1u << 12;
constexpr uint32_t kRomDisable = 1u << 13;

constexpr uint32_t kTransferStart = 1u << 0;
constexpr uint32_t kDma = 1u << 1;
constexpr uint32_t kReadWriteShift = 2;
constexpr uint32_t kLengthShift = 4;
constexpr uint32_t kRead = 0;
constexpr uint32_t kWrite = 1;

constexpr uint32_t kFirstInterrupt = 9;
constexpr uint32_t kInterruptsPerChannel = 3;

constexpr uint32_t kSramBase = 0x800000;
constexpr uint32_t kSramSize = 0x44;
constexpr uint32_t kUartBase = 0x800400;
constexpr uint32_t kUartSize = 0x50;
constexpr uint32_t kWiiRtcBase = 0x840000;
constexpr uint32_t kWiiRtcFlags = 0x20;
constexpr uint32_t kEuartBase = 0xC00000;
constexpr uint32_t kEuartSize = 8;
constexpr uint32_t kSecondsFrom1970To2000 = 946684800;

constexpr std::array<uint8_t, kSramSize> kDefaultSram = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0xFF, 0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 'D',  'O',  'L',  'P',  'H',  'I',  'N',  'S',  'L',  'O',  'T',  'A',
    'D',  'O',  'L',  'P',  'H',  'I',  'N',  'S',  'L',  'O',  'T',  'B',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6E, 0x6D, 0x00, 0x00, 0x00, 0x00,
};

class Ipl {
public:
    void select() {
        command_bytes_ = 0;
        command_ = 0;
        cursor_ = 0;
    }

    void deselect() {
        if (sram_dirty_) {
            save_sram();
            sram_dirty_ = false;
        }
    }

    void transfer(uint8_t& data) {
        if (command_bytes_ < 4) {
            command_ = (command_ << 8) | data;
            data = 0xFF;
            if (++command_bytes_ == 4) {
                update_rtc();
            }
            return;
        }
        bool write = (command_ >> 31) != 0;
        uint32_t address = (command_ >> 6) & 0x1FFFFFF;
        if (address >= kSramBase && address < kSramBase + kSramSize) {
            uint32_t offset = (address - kSramBase + cursor_++) % kSramSize;
            if (write) {
                sram_dirty_ = sram_dirty_ || sram_[offset] != data;
                sram_[offset] = data;
            } else {
                data = sram_[offset];
            }
        } else if (address >= kUartBase && address < kUartBase + kUartSize) {
            if (address == kUartBase) {
                uart(write, data);
            }
        } else if (address == kWiiRtcBase + kWiiRtcFlags) {
            if (write) {
                rtc_flags_ = data;
            } else {
                data = rtc_flags_;
            }
        } else if (address >= kEuartBase && address < kEuartBase + kEuartSize) {
            if (address == kEuartBase + 4) {
                uart(write, data);
            }
        } else if (!write) {
            data = 0;
        }
    }

    void load(const std::string& file) {
        sram_file_ = file;
        sram_ = kDefaultSram;
        std::ifstream in(file, std::ios::binary);
        std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (saved.size() == kSramSize) {
            std::copy(saved.begin(), saved.end(), sram_.begin());
        }
    }

private:
    void update_rtc() {
        std::time_t now = std::time(nullptr);
        std::tm local = *std::localtime(&now);
        std::tm utc = *std::gmtime(&now);
        utc.tm_isdst = local.tm_isdst;
        long long offset = static_cast<long long>(std::difftime(std::mktime(&local), std::mktime(&utc)));
        uint32_t rtc = static_cast<uint32_t>(static_cast<long long>(now) + offset - kSecondsFrom1970To2000);
        sram_[0] = static_cast<uint8_t>(rtc >> 24);
        sram_[1] = static_cast<uint8_t>(rtc >> 16);
        sram_[2] = static_cast<uint8_t>(rtc >> 8);
        sram_[3] = static_cast<uint8_t>(rtc);
    }

    void uart(bool write, uint8_t& data) {
        if (!write) {
            data = 0;
            return;
        }
        if (data == '\r') {
            return;
        }
        if (data != 0) {
            line_.push_back(static_cast<char>(data));
        }
        if (data == '\n' || line_.size() >= 4096) {
            std::fputs(line_.c_str(), stderr);
            line_.clear();
        }
    }

    void save_sram() {
        if (sram_file_.empty()) {
            return;
        }
        std::ofstream out(sram_file_, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(sram_.data()), kSramSize);
    }

    uint32_t command_ = 0;
    uint32_t command_bytes_ = 0;
    uint32_t cursor_ = 0;
    uint8_t rtc_flags_ = 0;
    std::array<uint8_t, kSramSize> sram_ = kDefaultSram;
    bool sram_dirty_ = false;
    std::string sram_file_;
    std::string line_;
};

struct Channel {
    uint32_t status = 0;
    uint32_t dma_address = 0;
    uint32_t dma_length = 0;
    uint32_t control = 0;
    uint32_t data = 0;
};

Ipl g_ipl;
std::array<Channel, kChannels> g_channels = [] {
    std::array<Channel, kChannels> channels{};
    channels[0].status = kExtInt;
    channels[1].status = kExtInt | (1u << kChipSelectShift);
    return channels;
}();

bool ipl_selected(uint32_t channel, uint32_t chip_select) {
    return channel == 0 && chip_select == 2;
}

uint8_t transfer_byte(uint32_t channel, uint32_t chip_select, uint8_t byte) {
    if (ipl_selected(channel, chip_select)) {
        g_ipl.transfer(byte);
        return byte;
    }
    return 0;
}

void run_transfer(uint32_t index) {
    Channel& channel = g_channels[index];
    uint32_t chip_select = (channel.status & kChipSelectMask) >> kChipSelectShift;
    if (chip_select != 1 && chip_select != 2 && chip_select != 4) {
        return;
    }
    uint32_t mode = (channel.control >> kReadWriteShift) & 3;
    if (channel.control & kDma) {
        for (uint32_t i = 0; i < channel.dma_length; i++) {
            uint32_t address = channel.dma_address + i;
            uint8_t byte = mode == kWrite ? rd8(address) : 0;
            byte = transfer_byte(index, chip_select, byte);
            if (mode == kRead) {
                wr8(address, byte);
            }
        }
    } else {
        uint32_t length = ((channel.control >> kLengthShift) & 3) + 1;
        if (mode == kWrite) {
            uint32_t data = channel.data;
            for (uint32_t i = 0; i < length; i++) {
                transfer_byte(index, chip_select, static_cast<uint8_t>(data >> 24));
                data <<= 8;
            }
        } else if (mode == kRead) {
            uint32_t result = 0;
            for (uint32_t i = 0; i < length; i++) {
                result |= static_cast<uint32_t>(transfer_byte(index, chip_select, 0)) << (24 - i * 8);
            }
            channel.data = result;
        }
    }
    channel.control &= ~kTransferStart;
    channel.status |= kTcInt;
}

void write_status(uint32_t index, uint32_t value) {
    Channel& channel = g_channels[index];
    uint32_t status = channel.status;
    status = (status & ~kExiIntMask) | (value & kExiIntMask);
    if (value & kExiInt) {
        status &= ~kExiInt;
    }
    status = (status & ~kTcIntMask) | (value & kTcIntMask);
    if (value & kTcInt) {
        status &= ~kTcInt;
    }
    status = (status & ~kClockMask) | (value & kClockMask);
    if (index < 2) {
        status = (status & ~kExtIntMask) | (value & kExtIntMask);
        if (value & kExtInt) {
            status &= ~kExtInt;
        }
    }
    if (index == 0) {
        status = (status & ~kRomDisable) | (value & kRomDisable);
    }
    uint32_t old_select = (channel.status & kChipSelectMask) >> kChipSelectShift;
    uint32_t new_select = (value & kChipSelectMask) >> kChipSelectShift;
    status = (status & ~kChipSelectMask) | (value & kChipSelectMask);
    channel.status = status;
    if (index == 0 && (old_select ^ new_select) == 2) {
        if (new_select == 2) {
            g_ipl.select();
        } else {
            g_ipl.deselect();
        }
    }
}

}

void mount(const std::string& sram_file) {
    g_ipl.load(sram_file);
}

uint32_t read32(uint32_t address) {
    uint32_t offset = address - kBase;
    uint32_t index = offset / kChannelStride;
    if (index >= kChannels) {
        return 0;
    }
    Channel& channel = g_channels[index];
    switch (offset % kChannelStride) {
    case kStatus:
        return channel.status & ~kExt;
    case kDmaAddress:
        return channel.dma_address;
    case kDmaLength:
        return channel.dma_length;
    case kControl:
        return channel.control;
    case kData:
        return channel.data;
    default:
        return 0;
    }
}

void write32(uint32_t address, uint32_t value) {
    uint32_t offset = address - kBase;
    uint32_t index = offset / kChannelStride;
    if (index >= kChannels) {
        return;
    }
    Channel& channel = g_channels[index];
    switch (offset % kChannelStride) {
    case kStatus:
        write_status(index, value);
        break;
    case kDmaAddress:
        channel.dma_address = value;
        break;
    case kDmaLength:
        channel.dma_length = value;
        break;
    case kControl:
        channel.control = value;
        if (value & kTransferStart) {
            run_transfer(index);
        }
        break;
    case kData:
        channel.data = value;
        break;
    default:
        break;
    }
}

uint32_t pending_interrupt() {
    for (uint32_t index = 0; index < kChannels; index++) {
        uint32_t status = g_channels[index].status;
        uint32_t first = kFirstInterrupt + index * kInterruptsPerChannel;
        if ((status & kExiInt) && (status & kExiIntMask)) {
            return first;
        }
        if ((status & kTcInt) && (status & kTcIntMask)) {
            return first + 1;
        }
        if (index < 2 && (status & kExtInt) && (status & kExtIntMask)) {
            return first + 2;
        }
    }
    return 0;
}

}
