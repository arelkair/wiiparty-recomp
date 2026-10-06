#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace wp::real_wiimote {

using Sender = std::function<void(const std::vector<uint8_t>&)>;

void start();
bool present(uint32_t slot);
bool take_wake(uint32_t slot);
void reset(uint32_t slot);
void output_report(uint32_t slot, const uint8_t* data, uint32_t size);
void update(uint32_t slot, const Sender& send);

}
