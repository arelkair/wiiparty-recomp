#pragma once

#include <cstddef>
#include <cstdint>

namespace wp::gamepad {

constexpr size_t kSlots = 4;
constexpr uint32_t kMenuUp = 0x01;
constexpr uint32_t kMenuDown = 0x02;
constexpr uint32_t kMenuLeft = 0x04;
constexpr uint32_t kMenuRight = 0x08;
constexpr uint32_t kMenuAccept = 0x10;
constexpr uint32_t kMenuCancel = 0x20;
constexpr uint32_t kMenuToggle = 0x40;

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
uint32_t menu_buttons();

}
