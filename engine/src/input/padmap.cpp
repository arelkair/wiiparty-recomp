#include "wp/padmap.h"

#include "wp/input.h"
#include "wp/settings.h"

namespace wp::padmap {

namespace {

struct Action {
    const char* name;
    uint32_t button;
};

constexpr Action kActions[kActionCount] = {
    {"a", input::kButtonA},         {"b", input::kButtonB},         {"one", input::kButtonOne}, {"two", input::kButtonTwo},
    {"plus", input::kButtonPlus}, {"minus", input::kButtonMinus}, {"home", input::kButtonHome},
};

constexpr const char* kPhysicalNames[kPhysicalCount] = {
    "south", "east", "west", "north", "back", "guide", "start", "left_stick", "right_stick",
    "left_shoulder", "right_shoulder", "dpad_up", "dpad_down", "dpad_left", "dpad_right", "left_trigger", "right_trigger",
};

uint32_t automatic(const Pressed& pressed, bool sideways) {
    auto held = [&](Physical physical) { return pressed.held[physical]; };
    uint32_t b = 0;
    if (held(kRightShoulder)) {
        b |= input::kButtonA;
    }
    if (held(kRightTrigger)) {
        b |= input::kButtonB;
    }
    if (sideways) {
        if (held(kSouth)) {
            b |= input::kButtonTwo;
        }
        if (held(kWest) || held(kEast)) {
            b |= input::kButtonOne;
        }
        if (held(kNorth)) {
            b |= input::kButtonA;
        }
    } else {
        if (held(pressed.nintendo_layout ? kEast : kSouth)) {
            b |= input::kButtonA;
        }
        if (held(pressed.nintendo_layout ? kSouth : kEast)) {
            b |= input::kButtonB;
        }
        if (held(kWest)) {
            b |= input::kButtonOne;
        }
        if (held(kNorth)) {
            b |= input::kButtonTwo;
        }
    }
    if (held(kBack)) {
        b |= input::kButtonMinus;
    }
    if (held(kStart)) {
        b |= input::kButtonPlus;
    }
    if (held(kGuide)) {
        b |= input::kButtonHome;
    }
    return b;
}

}

const char* action_name(size_t action) {
    return action < kActionCount ? kActions[action].name : "";
}

uint32_t action_button(size_t action) {
    return action < kActionCount ? kActions[action].button : 0;
}

std::string setting_key(size_t action) {
    return std::string("pad.") + action_name(action);
}

const char* physical_name(int physical) {
    return physical >= 0 && physical < kPhysicalCount ? kPhysicalNames[physical] : "auto";
}

int physical_from_name(const std::string& name) {
    for (int i = 0; i < kPhysicalCount; i++) {
        if (name == kPhysicalNames[i]) {
            return i;
        }
    }
    return kAutomatic;
}

Mapping current_mapping() {
    Mapping mapping{};
    for (size_t i = 0; i < kActionCount; i++) {
        mapping[i] = physical_from_name(settings::text(setting_key(i).c_str(), nullptr));
    }
    return mapping;
}

uint32_t buttons(const Pressed& pressed, bool sideways, const Mapping& mapping) {
    bool custom = false;
    for (int physical : mapping) {
        custom = custom || physical != kAutomatic;
    }
    if (!custom || sideways) {
        return automatic(pressed, sideways);
    }
    Pressed unmapped = pressed;
    for (int physical : mapping) {
        if (physical != kAutomatic) {
            unmapped.held[physical] = false;
        }
    }
    uint32_t defaults = automatic(unmapped, false);
    uint32_t b = 0;
    for (size_t i = 0; i < kActionCount; i++) {
        if (mapping[i] == kAutomatic) {
            b |= defaults & kActions[i].button;
        } else if (pressed.held[mapping[i]]) {
            b |= kActions[i].button;
        }
    }
    return b;
}

}
