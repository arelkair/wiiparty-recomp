#include "wp/determinism.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "wp/log.h"
#include "wp/memory.h"
#include "wp/vclock.h"

namespace wp::determinism {

namespace {

constexpr uint32_t kMem1Size = 0x01800000;
constexpr uint32_t kMem2Base = 0x10000000;
constexpr uint32_t kMem2Size = 0x04000000;
constexpr size_t kChunk = 1u << 20;

uint64_t mix(uint64_t hash, uint64_t word) {
    hash ^= word;
    hash *= 0x100000001B3ull;
    hash ^= hash >> 29;
    return hash;
}

uint64_t hash_range(const uint8_t* data, size_t size) {
    uint64_t hash = 0xCBF29CE484222325ull;
    for (size_t i = 0; i + 8 <= size; i += 8) {
        uint64_t word;
        std::memcpy(&word, data + i, sizeof word);
        hash = mix(hash, word);
    }
    return hash;
}

}

void retrace(uint64_t index) {
    static const uint64_t every = [] {
        const char* value = std::getenv("WP_DETERMINISM_EVERY");
        return value ? static_cast<uint64_t>(std::strtoull(value, nullptr, 10)) : 0ull;
    }();
    if (every == 0 || !vclock::deterministic() || index % every != 0) {
        return;
    }
    uint64_t total = 0xCBF29CE484222325ull;
    std::string chunks;
    char text[16];
    for (uint32_t offset = 0; offset < kMem1Size; offset += kChunk) {
        uint64_t part = hash_range(g_memory + offset, kChunk);
        total = mix(total, part);
        std::snprintf(text, sizeof text, "%08x ", static_cast<uint32_t>(part));
        chunks += text;
    }
    for (uint32_t offset = 0; offset < kMem2Size; offset += kChunk) {
        uint64_t part = hash_range(g_memory + kMem2Base + offset, kChunk);
        total = mix(total, part);
        std::snprintf(text, sizeof text, "%08x ", static_cast<uint32_t>(part));
        chunks += text;
    }
    log::write("determinism", "retrace %llu virtual %lld ns total %016llx", static_cast<unsigned long long>(index), static_cast<long long>(vclock::elapsed_ns()),
               static_cast<unsigned long long>(total));
    log::write("determinism", "chunks %llu %s", static_cast<unsigned long long>(index), chunks.c_str());
}

}
