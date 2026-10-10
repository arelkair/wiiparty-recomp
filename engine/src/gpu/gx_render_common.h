#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "wp/gx_render.h"
#include "wp/gx_state.h"

namespace wp::gx::render {

constexpr int kEfbWidth = 640;
constexpr int kEfbHeight = 528;
constexpr uint32_t kMaxStages = 16;
constexpr uint32_t kTextureMaps = 8;
constexpr float kScissorOffset = 342.0f;
constexpr uint32_t kTlutSize = 0x100000;
constexpr uint32_t kTlutMask = 0x7FE00;
constexpr uint32_t kXfbFormat = 13;

struct Constants {
    float initial[4][4];
    float konst[4][4];
    uint32_t stage[kMaxStages][4];
    uint32_t header[4];
    uint32_t swaps[4];
    float texdims[8][4];
    int32_t indmtx[6][4];
    uint32_t indirect[4];
    uint32_t tevind[kMaxStages][4];
    float fog[4];
    int32_t fog_integer[4];
    float fog_range[3][4];
};

struct Layout {
    uint32_t block_width;
    uint32_t block_height;
    bool supported;
};

struct AlphaTestResult {
    bool can_pass;
    bool can_fail;
};

struct TextureRequest {
    bool valid = false;
    uint32_t address = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint32_t levels = 1;
    size_t size = 0;
    size_t total = 0;
    size_t offsets[12] = {};
    const uint8_t* source = nullptr;
    const uint8_t* tlut = nullptr;
    uint32_t tlut_format = 0;
    uint32_t entries = 0;
    uint64_t key = 0;
    bool mipmaps = false;
    bool supported = true;
};

struct SamplerParams {
    uint32_t wrap_s = 0;
    uint32_t wrap_t = 0;
    bool min_linear = false;
    bool mag_linear = false;
    uint32_t mip_mode = 0;
    float min_lod = 0.0f;
    float max_lod = 0.0f;
    float lod_bias = 0.0f;
    bool unlimited_lod = false;
    uint64_t key = 0;
};

extern uint8_t g_tlut[kTlutSize];

TextureRequest describe_texture(uint32_t map);
uint64_t texture_hash(const TextureRequest& request);
std::vector<std::vector<uint32_t>> decode_levels(const TextureRequest& request);
SamplerParams sampler_params(uint32_t map, bool custom);

Layout layout_for(uint32_t format);
uint32_t block_bytes(uint32_t format);
std::vector<uint32_t> decode_texture(const uint8_t* data, uint32_t width, uint32_t height, uint32_t format, const uint8_t* tlut, uint32_t tlut_format);
uint64_t hash_bytes(const uint8_t* data, size_t size);
uint64_t sample_hash(const uint8_t* data, size_t size);
BlendState blend_state_for(uint32_t word);
void fill_constants(Constants& constants, int scale);
AlphaTestResult alpha_test_result(uint32_t test);
uint32_t copy_bits(uint32_t format);
uint32_t copy_texture_format(uint32_t format);
inline uint32_t scale_bits(uint32_t value, int bits) {
    return (value * ((1u << bits) - 1) + 127) / 255;
}

inline void encode_texel(uint8_t* block, uint32_t format, uint32_t index, const uint8_t* rgba) {
    uint32_t r = rgba[0];
    uint32_t g = rgba[1];
    uint32_t b = rgba[2];
    uint32_t a = rgba[3];
    auto put16 = [&](uint32_t offset, uint32_t value) {
        block[offset] = static_cast<uint8_t>(value >> 8);
        block[offset + 1] = static_cast<uint8_t>(value);
    };
    switch (format) {
    case 0: {
        uint8_t& byte = block[index / 2];
        uint32_t nibble = scale_bits(r, 4);
        byte = (index & 1) ? static_cast<uint8_t>((byte & 0xF0) | nibble) : static_cast<uint8_t>((byte & 0x0F) | (nibble << 4));
        break;
    }
    case 1:
        block[index] = static_cast<uint8_t>(r);
        break;
    case 2:
        block[index] = static_cast<uint8_t>((scale_bits(a, 4) << 4) | scale_bits(r, 4));
        break;
    case 3:
        block[index * 2] = static_cast<uint8_t>(a);
        block[index * 2 + 1] = static_cast<uint8_t>(r);
        break;
    case 4:
        put16(index * 2, (scale_bits(r, 5) << 11) | (scale_bits(g, 6) << 5) | scale_bits(b, 5));
        break;
    case 5: {
        uint32_t alpha = scale_bits(a, 3);
        put16(index * 2, alpha == 7 ? 0x8000 | (scale_bits(r, 5) << 10) | (scale_bits(g, 5) << 5) | scale_bits(b, 5)
                                    : (alpha << 12) | (scale_bits(r, 4) << 8) | (scale_bits(g, 4) << 4) | scale_bits(b, 4));
        break;
    }
    default:
        block[index * 2] = static_cast<uint8_t>(a);
        block[index * 2 + 1] = static_cast<uint8_t>(r);
        block[32 + index * 2] = static_cast<uint8_t>(g);
        block[32 + index * 2 + 1] = static_cast<uint8_t>(b);
        break;
    }
}

uint32_t expand(uint32_t value, int bits);
uint32_t quantize(uint32_t value, int bits);

}
