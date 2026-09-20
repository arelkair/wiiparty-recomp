#include "wp/gx.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "wp/memory.h"

namespace wp::gx {

namespace {

constexpr size_t kFlushThreshold = 1u << 20;
constexpr uint8_t kCommandNop = 0x00;
constexpr uint8_t kCommandLoadCp = 0x08;
constexpr uint8_t kCommandLoadXf = 0x10;
constexpr uint8_t kCommandCallList = 0x40;
constexpr uint8_t kCommandMetrics = 0x44;
constexpr uint8_t kCommandInvalidate = 0x48;
constexpr uint8_t kCommandLoadBp = 0x61;
constexpr uint8_t kCommandDrawMask = 0x80;
constexpr uint8_t kBpCopyExecute = 0x52;
constexpr uint32_t kCopyToFramebuffer = 1u << 14;
constexpr int kMaxLoggedCopies = 60;

enum Attribute : uint32_t { kNone = 0, kDirect = 1, kIndex8 = 2, kIndex16 = 3 };

struct Statistics {
    uint32_t draws = 0;
    uint32_t vertices = 0;
    uint32_t bp_writes = 0;
    uint32_t cp_writes = 0;
    uint32_t xf_words = 0;
    uint32_t lists = 0;
    uint32_t unknown = 0;
};

std::vector<uint8_t> g_fifo;
uint32_t g_vcd_low = 0;
uint32_t g_vcd_high = 0;
uint32_t g_vat[3][8] = {};
uint32_t g_bp[256] = {};
Statistics g_stats;
int g_logged_copies = 0;
bool g_log = std::getenv("WP_LOG_GX") != nullptr;

uint32_t be32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

uint32_t be16(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 8) | data[1];
}

uint32_t component_size(uint32_t format) {
    switch (format) {
    case 0:
    case 1:
        return 1;
    case 2:
    case 3:
        return 2;
    default:
        return 4;
    }
}

uint32_t attribute_size(uint32_t mode, uint32_t direct) {
    switch (mode) {
    case kDirect:
        return direct;
    case kIndex8:
        return 1;
    case kIndex16:
        return 2;
    default:
        return 0;
    }
}

uint32_t color_size(uint32_t format) {
    static const uint32_t kSizes[] = {2, 3, 4, 2, 3, 4, 4, 4};
    return kSizes[format & 7];
}

uint32_t vertex_size(uint32_t vat) {
    uint32_t a = g_vat[0][vat];
    uint32_t b = g_vat[1][vat];
    uint32_t c = g_vat[2][vat];
    uint32_t size = 0;
    size += g_vcd_low & 1;
    size += __builtin_popcount((g_vcd_low >> 1) & 0xFF);
    uint32_t position_components = (a & 1) ? 3 : 2;
    size += attribute_size((g_vcd_low >> 9) & 3, position_components * component_size((a >> 1) & 7));
    uint32_t normal_components = (a & 0x200) ? 9 : 3;
    size += attribute_size((g_vcd_low >> 11) & 3, normal_components * component_size((a >> 10) & 7));
    size += attribute_size((g_vcd_low >> 13) & 3, color_size((a >> 14) & 7));
    size += attribute_size((g_vcd_low >> 15) & 3, color_size((a >> 18) & 7));
    uint32_t counts[8] = {(a >> 21) & 1, b & 1, (b >> 9) & 1, (b >> 18) & 1, (b >> 27) & 1, (c >> 5) & 1, (c >> 14) & 1, (c >> 23) & 1};
    uint32_t formats[8] = {(a >> 22) & 7, (b >> 1) & 7, (b >> 10) & 7, (b >> 19) & 7, (b >> 28) & 7, (c >> 6) & 7, (c >> 15) & 7, (c >> 24) & 7};
    for (int i = 0; i < 8; i++) {
        size += attribute_size((g_vcd_high >> (2 * i)) & 3, (counts[i] + 1) * component_size(formats[i]));
    }
    return size;
}

void report_copy() {
    if (!g_log || g_logged_copies >= kMaxLoggedCopies) {
        return;
    }
    g_logged_copies++;
    std::fprintf(stderr, "GX copy: draws=%u vertices=%u bp=%u cp=%u xf=%u lists=%u unknown=%u\n", g_stats.draws,
                 g_stats.vertices, g_stats.bp_writes, g_stats.cp_writes, g_stats.xf_words, g_stats.lists,
                 g_stats.unknown);
    g_stats = Statistics{};
}

void load_bp(uint32_t word) {
    uint32_t reg = word >> 24;
    uint32_t value = word & 0xFFFFFF;
    g_bp[reg] = value;
    g_stats.bp_writes++;
    if (reg == kBpCopyExecute && (value & kCopyToFramebuffer)) {
        report_copy();
    }
}

void load_cp(uint8_t reg, uint32_t value) {
    g_stats.cp_writes++;
    if (reg == 0x50) {
        g_vcd_low = value;
    } else if (reg == 0x60) {
        g_vcd_high = value;
    } else if (reg >= 0x70 && reg < 0x98) {
        g_vat[(reg >> 4) - 7][reg & 7] = value;
    }
}

size_t parse(const uint8_t* data, size_t size, bool list);

size_t parse_one(const uint8_t* data, size_t size, bool list) {
    uint8_t command = data[0];
    if (command == kCommandNop || command == kCommandMetrics || command == kCommandInvalidate) {
        return 1;
    }
    if (command == kCommandLoadCp) {
        if (size < 6) {
            return 0;
        }
        load_cp(data[1], be32(data + 2));
        return 6;
    }
    if (command == kCommandLoadXf) {
        if (size < 5) {
            return 0;
        }
        uint32_t count = (be32(data + 1) >> 16) + 1;
        if (size < 5 + 4 * static_cast<size_t>(count)) {
            return 0;
        }
        g_stats.xf_words += count;
        return 5 + 4 * static_cast<size_t>(count);
    }
    if ((command & 0xE7) == 0x20) {
        if (size < 5) {
            return 0;
        }
        g_stats.xf_words++;
        return 5;
    }
    if (command == kCommandCallList) {
        if (size < 9) {
            return 0;
        }
        uint32_t address = be32(data + 1);
        uint32_t length = be32(data + 5);
        g_stats.lists++;
        if (!list) {
            parse(host(address), length, true);
        }
        return 9;
    }
    if (command == kCommandLoadBp) {
        if (size < 5) {
            return 0;
        }
        load_bp(be32(data + 1));
        return 5;
    }
    if (command & kCommandDrawMask) {
        if (size < 3) {
            return 0;
        }
        uint32_t count = be16(data + 1);
        size_t total = 3 + static_cast<size_t>(count) * vertex_size(command & 7);
        if (size < total) {
            return 0;
        }
        g_stats.draws++;
        g_stats.vertices += count;
        return total;
    }
    g_stats.unknown++;
    return 1;
}

size_t parse(const uint8_t* data, size_t size, bool list) {
    size_t offset = 0;
    while (offset < size) {
        size_t used = parse_one(data + offset, size - offset, list);
        if (used == 0) {
            break;
        }
        offset += used;
    }
    return offset;
}

}

void push(uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; i++) {
        g_fifo.push_back(static_cast<uint8_t>(value >> (8 * (bytes - 1 - i))));
    }
    if (g_fifo.size() >= kFlushThreshold) {
        process();
    }
}

void process() {
    if (g_fifo.empty()) {
        return;
    }
    size_t used = parse(g_fifo.data(), g_fifo.size(), false);
    g_fifo.erase(g_fifo.begin(), g_fifo.begin() + static_cast<std::ptrdiff_t>(used));
}

}
