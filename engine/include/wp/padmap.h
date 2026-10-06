#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace wp::padmap {

enum Physical {
    kSouth,
    kEast,
    kWest,
    kNorth,
    kBack,
    kGuide,
    kStart,
    kLeftStick,
    kRightStick,
    kLeftShoulder,
    kRightShoulder,
    kDpadUp,
    kDpadDown,
    kDpadLeft,
    kDpadRight,
    kLeftTrigger,
    kRightTrigger,
    kPhysicalCount
};

constexpr int kAutomatic = -1;
constexpr size_t kActionCount = 7;

struct Pressed {
    std::array<bool, kPhysicalCount> held{};
    bool nintendo_layout = false;
};

using Mapping = std::array<int, kActionCount>;

const char* action_name(size_t action);
uint32_t action_button(size_t action);
std::string setting_key(size_t action);
const char* physical_name(int physical);
int physical_from_name(const std::string& name);
Mapping current_mapping();
uint32_t buttons(const Pressed& pressed, bool sideways, const Mapping& mapping);

}
