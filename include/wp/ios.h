#pragma once

#include <cstdint>
#include <string>

namespace wp::ios {

int32_t send(uint32_t request);
int32_t open(const std::string& path, uint32_t mode = 1);
int32_t ioctl(int32_t descriptor, uint32_t command, uint32_t input, uint32_t input_size, uint32_t output,
              uint32_t output_size);
int32_t ioctlv(int32_t descriptor, uint32_t command, uint32_t input_count, uint32_t output_count,
               uint32_t vectors);

}
