#include "wp/determinism.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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

constexpr size_t kPage = 4096;
constexpr size_t kPageCount = 0x14000000 / kPage;
uint8_t g_dsp_pages[kPageCount] = {};

uint64_t hash_range(const uint8_t* data, size_t size) {
    uint64_t hash = 0xCBF29CE484222325ull;
    size_t first_page = static_cast<size_t>(data - g_memory) / kPage;
    for (size_t i = 0; i + 8 <= size; i += 8) {
        if (g_dsp_pages[first_page + i / kPage]) {
            i = (i / kPage + 1) * kPage - 8;
            continue;
        }
        uint64_t word;
        std::memcpy(&word, data + i, sizeof word);
        hash = mix(hash, word);
    }
    return hash;
}

}

void note_dsp_write(uint32_t physical_offset, uint32_t size) {
    if (!vclock::deterministic() || size == 0) {
        return;
    }
    for (uint64_t page = physical_offset / kPage; page <= (static_cast<uint64_t>(physical_offset) + size - 1) / kPage && page < kPageCount; page++) {
        g_dsp_pages[page] = 1;
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
    static const int snapshot_chunk = [] {
        const char* value = std::getenv("WP_DETERMINISM_CHUNK");
        return value ? std::atoi(value) : -1;
    }();
    std::vector<uint8_t> snapshot;
    const uint8_t* snapshot_base = nullptr;
    if (snapshot_chunk >= 0 && snapshot_chunk < 24 + 64) {
        snapshot_base = snapshot_chunk < 24 ? g_memory + static_cast<size_t>(snapshot_chunk) * kChunk : g_memory + kMem2Base + static_cast<size_t>(snapshot_chunk - 24) * kChunk;
        snapshot.assign(snapshot_base, snapshot_base + kChunk);
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
    static const int watched = [] {
        const char* value = std::getenv("WP_DETERMINISM_CHUNK");
        return value ? std::atoi(value) : -1;
    }();
    if (watched >= 0 && watched < 24 + 64) {
        const uint8_t* base = watched < 24 ? g_memory + static_cast<size_t>(watched) * kChunk : g_memory + kMem2Base + static_cast<size_t>(watched - 24) * kChunk;
        std::string pages;
        for (size_t page = 0; page < kChunk; page += 4096) {
            std::snprintf(text, sizeof text, "%08x ", static_cast<uint32_t>(hash_range(base + page, 4096)));
            pages += text;
            if ((page / 4096) % 32 == 31) {
                log::write("determinism", "pages %llu %zu %s", static_cast<unsigned long long>(index), page / 4096 - 31, pages.c_str());
                pages.clear();
            }
        }
    }
    if (snapshot_base) {
        int reported = 0;
        for (size_t i = 0; i < kChunk && reported < 8; i += 4) {
            if (std::memcmp(snapshot.data() + i, snapshot_base + i, 4) != 0) {
                log::write("determinism", "changed while hashing at chunk %d offset %zx: %02x%02x%02x%02x -> %02x%02x%02x%02x", snapshot_chunk, i, snapshot[i], snapshot[i + 1], snapshot[i + 2],
                           snapshot[i + 3], snapshot_base[i], snapshot_base[i + 1], snapshot_base[i + 2], snapshot_base[i + 3]);
                reported++;
            }
        }
    }
    static const long dump_page = [] {
        const char* value = std::getenv("WP_DETERMINISM_DUMP");
        return value ? std::atol(value) : -1;
    }();
    if (snapshot_base && dump_page >= 0 && dump_page < 256) {
        const uint8_t* page = snapshot_base + static_cast<size_t>(dump_page) * 4096;
        for (size_t line = 0; line < 4096; line += 128) {
            std::string hex;
            for (size_t i = 0; i < 128; i++) {
                std::snprintf(text, sizeof text, "%02x", page[line + i]);
                hex += text;
            }
            log::write("determinism", "dump %llu %zu %s", static_cast<unsigned long long>(index), line, hex.c_str());
        }
    }
    log::write("determinism", "retrace %llu virtual %lld ns total %016llx", static_cast<unsigned long long>(index), static_cast<long long>(vclock::elapsed_ns()),
               static_cast<unsigned long long>(total));
    size_t half = 44 * 9;
    log::write("determinism", "chunks %llu %s", static_cast<unsigned long long>(index), chunks.substr(0, half).c_str());
    log::write("determinism", "chunkz %llu %s", static_cast<unsigned long long>(index), chunks.substr(half).c_str());
}

}
