#pragma once

#include <cstdint>
#include <string>

namespace wp::exi {

void mount(const std::string& sram_file);
uint32_t read32(uint32_t address);
void write32(uint32_t address, uint32_t value);
uint32_t pending_interrupt();

}
