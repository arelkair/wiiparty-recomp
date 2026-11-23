#pragma once

#include <cstdint>

namespace wp::determinism {

void retrace(uint64_t index);
void note_dsp_write(uint32_t physical_offset, uint32_t size);

}
