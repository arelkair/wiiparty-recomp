#pragma once

#include <cstdint>

namespace wp::input {

constexpr uint32_t kButtonLeft = 0x0001;
constexpr uint32_t kButtonRight = 0x0002;
constexpr uint32_t kButtonDown = 0x0004;
constexpr uint32_t kButtonUp = 0x0008;
constexpr uint32_t kButtonPlus = 0x0010;
constexpr uint32_t kButtonTwo = 0x0100;
constexpr uint32_t kButtonOne = 0x0200;
constexpr uint32_t kButtonB = 0x0400;
constexpr uint32_t kButtonA = 0x0800;
constexpr uint32_t kButtonMinus = 0x1000;
constexpr uint32_t kButtonHome = 0x8000;

struct Sample {
    uint32_t buttons = 0;
    bool pointer_valid = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
};

bool connected(uint32_t channel);
Sample sample(uint32_t channel);

}
