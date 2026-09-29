#pragma once

#include <cstdint>
#include <vector>

namespace wp::video {

void start();
void present();
void update_statistics(double fps);
void save_next_frame(const char* path);
void* window_handle();
std::vector<uint8_t> encode_png(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height);
void save_png(const char* path, const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height);
void save_png_rgba(const char* path, const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height);
bool image_point(long x, long y, long client_width, long client_height, float& out_x, float& out_y);
bool window_focused();
bool client_size(int& width, int& height);
bool key_down(int key);
bool cursor_position(long& x, long& y);
bool cursor_on_image(float& x, float& y);

}
