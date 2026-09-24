#pragma once

namespace wp::video {

void start();
void present();
void update_statistics(double fps);
void save_next_frame(const char* path);
void* window_handle();
bool image_point(long x, long y, long client_width, long client_height, float& out_x, float& out_y);

}
