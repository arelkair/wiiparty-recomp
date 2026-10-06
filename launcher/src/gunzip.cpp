#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#include <stb_image.h>

#include "gunzip.h"

#include <cstdint>

namespace {

constexpr uint8_t kExtra = 4;
constexpr uint8_t kName = 8;
constexpr uint8_t kComment = 16;
constexpr uint8_t kHeaderCrc = 2;
constexpr size_t kHeaderSize = 10;
constexpr size_t kTrailerSize = 8;

}

std::vector<char> gunzip(std::string_view data) {
    std::vector<char> out;
    const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
    size_t size = data.size();
    if (size < kHeaderSize + kTrailerSize || bytes[0] != 0x1F || bytes[1] != 0x8B || bytes[2] != 8) {
        return out;
    }
    size_t end = size - kTrailerSize;
    size_t at = kHeaderSize;
    uint8_t flags = bytes[3];
    if ((flags & kExtra) && at + 2 <= end) {
        at += 2 + (bytes[at] | (bytes[at + 1] << 8));
    }
    for (uint8_t text : {kName, kComment}) {
        if (flags & text) {
            while (at < end && bytes[at] != 0) {
                at++;
            }
            at++;
        }
    }
    if (flags & kHeaderCrc) {
        at += 2;
    }
    if (at >= end) {
        return out;
    }
    uint32_t expected = bytes[end + 4] | (bytes[end + 5] << 8) | (bytes[end + 6] << 16) | (static_cast<uint32_t>(bytes[end + 7]) << 24);
    out.resize(expected);
    int written = stbi_zlib_decode_noheader_buffer(out.data(), static_cast<int>(expected), data.data() + at, static_cast<int>(end - at));
    if (written != static_cast<int>(expected)) {
        out.clear();
    }
    return out;
}
