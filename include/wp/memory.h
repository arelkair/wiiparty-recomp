#pragma once

#include <cstdint>
#include <cstring>

namespace wp {

constexpr uint32_t kAddressMask = 0x1FFFFFFF;
constexpr size_t kMemorySize = 0x14000000;

extern uint8_t* g_memory;

inline uint8_t* host(uint32_t address) {
    return g_memory + (address & kAddressMask);
}

inline uint8_t rd8(uint32_t address) {
    return *host(address);
}

inline uint16_t rd16(uint32_t address) {
    uint16_t value;
    std::memcpy(&value, host(address), sizeof value);
    return __builtin_bswap16(value);
}

inline uint32_t rd32(uint32_t address) {
    uint32_t value;
    std::memcpy(&value, host(address), sizeof value);
    return __builtin_bswap32(value);
}

inline uint64_t rd64(uint32_t address) {
    uint64_t value;
    std::memcpy(&value, host(address), sizeof value);
    return __builtin_bswap64(value);
}

inline uint16_t rd16_reversed(uint32_t address) {
    uint16_t value;
    std::memcpy(&value, host(address), sizeof value);
    return value;
}

inline uint32_t rd32_reversed(uint32_t address) {
    uint32_t value;
    std::memcpy(&value, host(address), sizeof value);
    return value;
}

inline void wr8(uint32_t address, uint8_t value) {
    *host(address) = value;
}

inline void wr16(uint32_t address, uint16_t value) {
    value = __builtin_bswap16(value);
    std::memcpy(host(address), &value, sizeof value);
}

inline void wr32(uint32_t address, uint32_t value) {
    value = __builtin_bswap32(value);
    std::memcpy(host(address), &value, sizeof value);
}

inline void wr64(uint32_t address, uint64_t value) {
    value = __builtin_bswap64(value);
    std::memcpy(host(address), &value, sizeof value);
}

inline void wr16_reversed(uint32_t address, uint16_t value) {
    std::memcpy(host(address), &value, sizeof value);
}

inline void wr32_reversed(uint32_t address, uint32_t value) {
    std::memcpy(host(address), &value, sizeof value);
}

inline float rdf32(uint32_t address) {
    uint32_t bits = rd32(address);
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

inline double rdf64(uint32_t address) {
    uint64_t bits = rd64(address);
    double value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

inline void wrf32(uint32_t address, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    wr32(address, bits);
}

inline void wrf64(uint32_t address, double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    wr64(address, bits);
}

inline void dcbz(uint32_t address) {
    std::memset(host(address & ~31u), 0, 32);
}

}
