#include <cstdlib>
#include <mutex>
#include <set>

#include "wp/dsp.h"
#include "wp/exi.h"
#include "wp/gx.h"
#include "wp/hollywood.h"
#include "wp/ipc.h"
#include "wp/log.h"
#include "wp/memory.h"
#include "wp/si.h"

namespace wp::mmio {

namespace {

bool dsp_register(uint32_t address) {
    return (address >> 12) == 0xCC005;
}

bool ipc_register(uint32_t address) {
    return (address >> 8) == 0xCD0000;
}

bool serial_register(uint32_t address) {
    return (address >> 12) == 0xCD006 && (address & 0xF00) == 0x400;
}

bool exi_register(uint32_t address) {
    return (address >> 12) == 0xCD006 && (address & 0xFC0) == 0x800;
}

void note_plain_access(uint32_t address, unsigned bytes, bool write) {
    static const bool enabled = std::getenv("WP_LOG_MMIO") != nullptr;
    if (!enabled || (address >> 24) < 0xCC || (address >> 24) > 0xCD) {
        return;
    }
    static std::mutex lock;
    static std::set<uint64_t> seen;
    std::lock_guard<std::mutex> hold(lock);
    uint64_t key = (static_cast<uint64_t>(address) << 8) | (bytes << 1) | (write ? 1u : 0u);
    if (seen.insert(key).second) {
        log::write("mmio", "plain memory %s%u %08x", write ? "write" : "read", bytes * 8, address);
    }
}

template <typename T, typename Swap>
T read_memory(uint32_t address, Swap swap) {
    T value;
    note_plain_access(address, sizeof value, false);
    std::memcpy(&value, host(address), sizeof value);
    return swap(value);
}

template <typename T, typename Swap>
void write_memory(uint32_t address, T value, Swap swap) {
    note_plain_access(address, sizeof value, true);
    value = swap(value);
    std::memcpy(host(address), &value, sizeof value);
}

}

uint16_t read16(uint32_t address) {
    if (dsp_register(address)) {
        return dsp::read16(address);
    }
    return read_memory<uint16_t>(address, [](uint16_t v) { return __builtin_bswap16(v); });
}

uint32_t read32(uint32_t address) {
    if (dsp_register(address)) {
        return (static_cast<uint32_t>(dsp::read16(address)) << 16) | dsp::read16(address + 2);
    }
    if (ipc_register(address)) {
        return ipc::read32(address);
    }
    if (serial_register(address)) {
        return si::read32(address);
    }
    if (hollywood::owns(address)) {
        return hollywood::read32(address);
    }
    if (exi_register(address)) {
        return exi::read32(address);
    }
    return read_memory<uint32_t>(address, [](uint32_t v) { return __builtin_bswap32(v); });
}

void write8(uint32_t address, uint8_t value) {
    if (address == kFifoAddress) {
        gx::push(value, 1);
        return;
    }
    note_plain_access(address, 1, true);
    *host(address) = value;
}

void write16(uint32_t address, uint16_t value) {
    if (address == kFifoAddress) {
        gx::push(value, 2);
        return;
    }
    if (address == 0xCC00100A) {
        gx::write_pixel_engine_control(value);
        return;
    }
    if (dsp_register(address)) {
        dsp::write16(address, value);
        return;
    }
    write_memory<uint16_t>(address, value, [](uint16_t v) { return __builtin_bswap16(v); });
}

void write32(uint32_t address, uint32_t value) {
    if (address == kFifoAddress) {
        gx::push(value, 4);
        return;
    }
    if (dsp_register(address)) {
        dsp::write16(address, static_cast<uint16_t>(value >> 16));
        dsp::write16(address + 2, static_cast<uint16_t>(value));
        return;
    }
    if (ipc_register(address)) {
        ipc::write32(address, value);
        return;
    }
    if (serial_register(address)) {
        si::write32(address, value);
        return;
    }
    if (hollywood::owns(address)) {
        hollywood::write32(address, value);
        return;
    }
    if (exi_register(address)) {
        exi::write32(address, value);
        return;
    }
    if (address == 0xCC001008) {
        write_memory<uint16_t>(address, static_cast<uint16_t>(value >> 16), [](uint16_t v) { return __builtin_bswap16(v); });
        gx::write_pixel_engine_control(static_cast<uint16_t>(value));
        return;
    }
    write_memory<uint32_t>(address, value, [](uint32_t v) { return __builtin_bswap32(v); });
}

void write64(uint32_t address, uint64_t value) {
    if (address == kFifoAddress) {
        gx::push(value, 8);
        return;
    }
    write_memory<uint64_t>(address, value, [](uint64_t v) { return __builtin_bswap64(v); });
}

}
