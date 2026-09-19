#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "functions.h"
#include "wp/cpu.h"

namespace {

constexpr uint32_t kObject = 0x80100000;
constexpr uint32_t kInner = 0x80101000;
constexpr uint32_t kText = 0x80102000;
constexpr uint32_t kOutputOffset = 0xBC;
constexpr uint32_t kOutputSize = 0x800;

void reference_quote_scan(uint32_t object) {
    uint32_t text = wp::rd32(wp::rd32(object + 4) + 8);
    uint32_t index = 0;
    uint32_t slot = 0;
    uint32_t stride = 0;
    bool open = false;
    for (uint32_t p = text; wp::rd16(p) != 0; p += 2) {
        if (wp::rd16(p) == 0x22) {
            if (open) {
                open = false;
            } else {
                uint32_t address = slot * 0x18 + stride + object + kOutputOffset;
                slot++;
                open = true;
                wp::wr32(address, text + (index + 1) * 2);
                if (slot == 10) {
                    slot = 0;
                    stride += 4;
                }
            }
        }
        index++;
    }
}

std::vector<uint8_t> snapshot_output() {
    const uint8_t* start = wp::host(kObject + kOutputOffset);
    return std::vector<uint8_t>(start, start + kOutputSize);
}

void write_text(const std::vector<uint16_t>& characters) {
    uint32_t address = kText;
    for (uint16_t value : characters) {
        wp::wr16(address, value);
        address += 2;
    }
    wp::wr16(address, 0);
}

bool run_quote_scan(const std::vector<uint16_t>& characters) {
    std::memset(wp::host(kObject), 0, 0x1000);
    wp::wr32(kObject + 4, kInner);
    wp::wr32(kInner + 8, kText);
    write_text(characters);

    wp::Cpu c{};
    c.r[3] = kObject;
    f_8007b190(c);
    std::vector<uint8_t> lifted = snapshot_output();

    bool expects_output = std::count(characters.begin(), characters.end(), 0x22) > 0;
    bool has_output = std::any_of(lifted.begin(), lifted.end(), [](uint8_t byte) { return byte != 0; });
    if (expects_output != has_output) {
        return false;
    }

    std::memset(wp::host(kObject + kOutputOffset), 0, kOutputSize);
    reference_quote_scan(kObject);
    return lifted == snapshot_output();
}

std::vector<uint16_t> quoted_text(int pairs) {
    std::vector<uint16_t> characters;
    for (int i = 0; i < pairs; i++) {
        characters.push_back('a' + i % 26);
        characters.push_back(0x22);
        characters.push_back('x');
        characters.push_back('y');
        characters.push_back(0x22);
    }
    return characters;
}

}

int main() {
    wp::g_memory = static_cast<uint8_t*>(std::calloc(wp::kMemorySize, 1));
    int failures = 0;
    for (int pairs : {0, 1, 3, 10, 11, 25}) {
        if (!run_quote_scan(quoted_text(pairs))) {
            std::fprintf(stderr, "quote scan mismatch with %d pairs\n", pairs);
            failures++;
        }
    }
    if (!run_quote_scan({0x22, 0x22, 0x22, 'q', 0x22, 0x22})) {
        std::fputs("quote scan mismatch on adjacent quotes\n", stderr);
        failures++;
    }
    std::free(wp::g_memory);
    if (failures == 0) {
        std::puts("all lifted tests passed");
    }
    return failures == 0 ? 0 : 1;
}
