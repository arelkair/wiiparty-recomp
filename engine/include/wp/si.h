#pragma once

#include <cstdint>

namespace wp::si {

uint32_t read32(uint32_t address);
void write32(uint32_t address, uint32_t value);
void poll();
bool interrupt_pending();

}
