#pragma once

#include <cstdint>

namespace wp::gx {

void process();
void last_framebuffer_size(uint32_t& width, uint32_t& height);
uint64_t framebuffer_copies();

}
