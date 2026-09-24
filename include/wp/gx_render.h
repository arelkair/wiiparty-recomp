#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

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

bool guest_range_valid(uint32_t address, size_t size);
void take_statistics(uint32_t& batches, uint32_t& vertices, double& seconds);

const char* api_name();
void draw(const ScreenVertex* vertices, uint32_t count);
void copy_to_framebuffer(int x, int y, int width, int height);
bool present_frame(void* window, double aspect);
bool read_frame(std::vector<uint32_t>& pixels, uint32_t& width, uint32_t& height);
void copy_to_texture(uint32_t address, int x, int y, int width, int height, bool half, uint32_t format, bool intensity);
void load_tlut(uint32_t address, uint32_t tmem_offset, uint32_t bytes);
void clear(int x, int y, int width, int height);

}
