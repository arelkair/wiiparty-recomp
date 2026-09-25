#pragma once

#include <cstdint>

namespace wp::gamepad {

struct State {
    bool connected = false;
    uint32_t buttons = 0;
    bool pointer_valid = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
    bool motion_valid = false;
    float accel[3] = {0.0f, 0.0f, 1.0f};
};

State poll(bool sideways);

}
