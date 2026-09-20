#pragma once

#include <cstdint>

namespace wp::gx {

struct ScreenVertex {
    float x;
    float y;
    float z;
    float color[2][4];
    float uv[8][2];
};

const uint32_t* bp_registers();
const uint32_t* konst_registers();
const uint32_t* xf_registers();

}

namespace wp::gx::render {

const char* api_name();
void draw(const ScreenVertex* vertices, uint32_t count);
void copy_to_framebuffer(uint32_t address, uint32_t stride, int x, int y, int width, int height);
void clear();

}
