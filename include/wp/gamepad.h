#pragma once

#include <cstddef>
#include <cstdint>

namespace wp::gamepad {

constexpr size_t kSlots = 4;

struct State {
    bool connected = false;
    uint32_t buttons = 0;
    bool pointer_valid = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
    bool motion_valid = false;
    float accel[3] = {0.0f, 0.0f, 1.0f};
};

bool connected(uint32_t slot);
State poll(uint32_t slot, bool sideways);
void set_outputs(uint32_t slot, bool rumble, uint8_t leds);

}
