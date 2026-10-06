#pragma once

#include <cstdint>

namespace wp::hollywood {

bool owns(uint32_t address);
uint32_t read32(uint32_t address);
void write32(uint32_t address, uint32_t value);

}
