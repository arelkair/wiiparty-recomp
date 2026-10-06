// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wp/si.h"

#include <array>

namespace wp::si {

namespace {

constexpr uint32_t kBase = 0xCD006400;
constexpr uint32_t kChannels = 4;
constexpr uint32_t kChannelStride = 0x0C;
constexpr uint32_t kChannelOut = 0x00;
constexpr uint32_t kChannelInHigh = 0x04;
constexpr uint32_t kChannelInLow = 0x08;
constexpr uint32_t kPoll = 0x30;
constexpr uint32_t kComCsr = 0x34;
constexpr uint32_t kStatus = 0x38;
constexpr uint32_t kExiClockCount = 0x3C;
constexpr uint32_t kBuffer = 0x80;
constexpr uint32_t kBufferSize = 0x80;

constexpr uint32_t kTransferStart = 1u << 0;
constexpr uint32_t kChannelShift = 1;
constexpr uint32_t kComKeptMask = (3u << 1) | (0x7Fu << 8) | (0x7Fu << 16) | (1u << 27) | (1u << 30);
constexpr uint32_t kReadStatusInterruptMask = 1u << 27;
constexpr uint32_t kReadStatusInterrupt = 1u << 28;
constexpr uint32_t kCommunicationError = 1u << 29;
constexpr uint32_t kTransferInterruptMask = 1u << 30;
constexpr uint32_t kTransferInterrupt = 1u << 31;

constexpr uint32_t kErrorLatch = 1u << 30;
constexpr uint32_t kErrorStatus = 1u << 31;
constexpr uint32_t kWrite = 1u << 31;
constexpr uint32_t kClearableErrors = 0x0F0F0F0F;

struct Channel {
    uint32_t out = 0;
    uint32_t in_high = 0;
    uint32_t in_low = 0;
};

std::array<Channel, kChannels> g_channels;
std::array<uint8_t, kBufferSize> g_buffer{};
uint32_t g_poll = 0;
uint32_t g_com = 0;
uint32_t g_status = 0;
uint32_t g_exi_clock_count = 0;

uint32_t shift_of(uint32_t channel) {
    return (kChannels - 1 - channel) * 8;
}

uint32_t no_response_bit(uint32_t channel) {
    return 1u << (shift_of(channel) + 3);
}

uint32_t read_status_bit(uint32_t channel) {
    return 1u << (shift_of(channel) + 5);
}

uint32_t write_status_bits() {
    uint32_t bits = 0;
    for (uint32_t channel = 0; channel < kChannels; channel++) {
        bits |= 1u << (shift_of(channel) + 4);
    }
    return bits;
}

void update_read_status() {
    bool ready = false;
    for (uint32_t channel = 0; channel < kChannels; channel++) {
        ready = ready || (g_status & read_status_bit(channel));
    }
    g_com = ready ? g_com | kReadStatusInterrupt : g_com & ~kReadStatusInterrupt;
}

void run_transfer() {
    uint32_t channel = (g_com >> kChannelShift) & 3;
    g_com &= ~kTransferStart;
    g_com |= kCommunicationError | kTransferInterrupt;
    g_status |= no_response_bit(channel);
}

}

uint32_t read32(uint32_t address) {
    uint32_t offset = address - kBase;
    if (offset >= kBuffer && offset < kBuffer + kBufferSize) {
        uint32_t index = offset - kBuffer;
        return (static_cast<uint32_t>(g_buffer[index]) << 24) | (static_cast<uint32_t>(g_buffer[index + 1]) << 16) |
               (static_cast<uint32_t>(g_buffer[index + 2]) << 8) | g_buffer[index + 3];
    }
    if (offset < kChannels * kChannelStride) {
        uint32_t channel = offset / kChannelStride;
        switch (offset % kChannelStride) {
        case kChannelOut:
            return g_channels[channel].out;
        case kChannelInHigh:
            g_status &= ~read_status_bit(channel);
            update_read_status();
            return g_channels[channel].in_high;
        case kChannelInLow:
            g_status &= ~read_status_bit(channel);
            update_read_status();
            return g_channels[channel].in_low;
        default:
            return 0;
        }
    }
    switch (offset) {
    case kPoll:
        return g_poll;
    case kComCsr:
        return g_com;
    case kStatus:
        return g_status;
    case kExiClockCount:
        return g_exi_clock_count;
    default:
        return 0;
    }
}

void write32(uint32_t address, uint32_t value) {
    uint32_t offset = address - kBase;
    if (offset >= kBuffer && offset < kBuffer + kBufferSize) {
        uint32_t index = offset - kBuffer;
        g_buffer[index] = static_cast<uint8_t>(value >> 24);
        g_buffer[index + 1] = static_cast<uint8_t>(value >> 16);
        g_buffer[index + 2] = static_cast<uint8_t>(value >> 8);
        g_buffer[index + 3] = static_cast<uint8_t>(value);
        return;
    }
    if (offset < kChannels * kChannelStride) {
        uint32_t channel = offset / kChannelStride;
        switch (offset % kChannelStride) {
        case kChannelOut:
            g_channels[channel].out = value;
            break;
        case kChannelInHigh:
            g_channels[channel].in_high = value;
            break;
        case kChannelInLow:
            g_channels[channel].in_low = value;
            break;
        default:
            break;
        }
        return;
    }
    switch (offset) {
    case kPoll:
        g_poll = value;
        break;
    case kComCsr:
        g_com = (g_com & ~kComKeptMask) | (value & kComKeptMask);
        if (value & kReadStatusInterrupt) {
            g_com &= ~kReadStatusInterrupt;
        }
        if (value & kTransferInterrupt) {
            g_com &= ~kTransferInterrupt;
        }
        if (value & kTransferStart) {
            g_com |= kTransferStart;
            run_transfer();
        }
        break;
    case kStatus:
        g_status &= ~(value & kClearableErrors);
        if (value & kWrite) {
            g_status &= ~(kWrite | write_status_bits());
        }
        break;
    case kExiClockCount:
        g_exi_clock_count = value;
        break;
    default:
        break;
    }
}

void poll() {
    for (uint32_t channel = 0; channel < kChannels; channel++) {
        g_status |= no_response_bit(channel);
        g_channels[channel].in_high |= kErrorStatus | kErrorLatch;
    }
    update_read_status();
}

bool interrupt_pending() {
    return ((g_com & kReadStatusInterrupt) && (g_com & kReadStatusInterruptMask)) ||
           ((g_com & kTransferInterrupt) && (g_com & kTransferInterruptMask));
}

}
