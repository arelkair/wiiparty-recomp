#include "app.h"

#include <cmath>

#include "keys.h"
#include "layout.h"
#include "texts.h"
#include "wp/input.h"
#include "wp/keymap.h"
#include "wp/padmap.h"

namespace {

constexpr float kStickThreshold = 0.5f;
constexpr float kTriggerThreshold = 0.5f;

const char* physical_label(int physical) {
    const Texts& t = texts();
    const char* labels[] = {t.pad_south,      t.pad_east,          t.pad_west,           t.pad_north,    t.pad_back,      t.pad_guide,
                            t.pad_start,      t.pad_left_stick,    t.pad_right_stick,    t.pad_left_shoulder, t.pad_right_shoulder,
                            t.pad_dpad_up,    t.pad_dpad_down,     t.pad_dpad_left,      t.pad_dpad_right,    t.pad_left_trigger, t.pad_right_trigger};
    return physical >= 0 && physical < wp::padmap::kPhysicalCount ? labels[physical] : t.pad_auto;
}

const char* wii_button_label(size_t action) {
    const char* labels[] = {"A", "B", "1", "2", "+", "−", "HOME"};
    return action < wp::padmap::kActionCount ? labels[action] : "";
}

std::string held_names(uint32_t buttons) {
    std::string text;
    auto add = [&](const char* name) {
        if (!text.empty()) {
            text += "  ·  ";
        }
        text += name;
    };
    for (size_t i = 0; i < wp::keymap::kActionCount; i++) {
        const wp::keymap::ActionInfo& info = wp::keymap::action(i);
        if (info.buttons != 0 && (buttons & info.buttons) == info.buttons) {
            add(action_name(info.name));
        }
    }
    return text;
}

}

void App::track_held(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        int code = wp::keymap::key_code(key_event_name(event.key));
        if (code != 0) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                held_codes_.insert(code);
            } else {
                held_codes_.erase(code);
            }
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        int code = wp::keymap::key_code(mouse_button_name(event.button.button));
        if (code != 0) {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                held_codes_.insert(code);
            } else {
                held_codes_.erase(code);
            }
        }
        break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        held_codes_.clear();
        break;
    case SDL_EVENT_GAMEPAD_REMOVED: {
        auto found = tester_pads_.find(event.gdevice.which);
        if (found != tester_pads_.end()) {
            if (found->second) {
                SDL_CloseGamepad(found->second);
            }
            tester_pads_.erase(found);
        }
        break;
    }
    default:
        break;
    }
}

uint32_t App::keyboard_buttons() {
    uint32_t buttons = 0;
    for (size_t i = 0; i < wp::keymap::kActionCount; i++) {
        for (int code : wp::keymap::parse(value(wp::keymap::setting_key(i))).codes) {
            if (held_codes_.count(code)) {
                buttons |= wp::keymap::action(i).buttons;
            }
        }
    }
    return buttons;
}

uint32_t App::gamepad_buttons(SDL_Gamepad* pad) {
    wp::padmap::Pressed pressed;
    pressed.nintendo_layout = SDL_GetGamepadButtonLabel(pad, SDL_GAMEPAD_BUTTON_SOUTH) == SDL_GAMEPAD_BUTTON_LABEL_B;
    for (int i = 0; i < wp::padmap::kLeftTrigger; i++) {
        pressed.held[i] = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(i));
    }
    pressed.held[wp::padmap::kLeftTrigger] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f > kTriggerThreshold;
    pressed.held[wp::padmap::kRightTrigger] = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f > kTriggerThreshold;
    wp::padmap::Mapping mapping{};
    for (size_t i = 0; i < wp::padmap::kActionCount; i++) {
        mapping[i] = wp::padmap::physical_from_name(value(wp::padmap::setting_key(i)));
    }
    uint32_t buttons = wp::padmap::buttons(pressed, false, mapping);
    float x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    float y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    if (pressed.held[wp::padmap::kDpadLeft] || x < -kStickThreshold) {
        buttons |= wp::input::kButtonLeft;
    }
    if (pressed.held[wp::padmap::kDpadRight] || x > kStickThreshold) {
        buttons |= wp::input::kButtonRight;
    }
    if (pressed.held[wp::padmap::kDpadUp] || y < -kStickThreshold) {
        buttons |= wp::input::kButtonUp;
    }
    if (pressed.held[wp::padmap::kDpadDown] || y > kStickThreshold) {
        buttons |= wp::input::kButtonDown;
    }
    return buttons;
}

void App::tester_section(float width) {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    ui::text(t.tester_title, Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    int count = 0;
    SDL_JoystickID* ids = gamepads(count);
    for (int player = 0; player < 4; player++) {
        SDL_Gamepad* pad = nullptr;
        if (player < count) {
            auto found = tester_pads_.find(ids[player]);
            pad = found != tester_pads_.end() ? found->second : SDL_OpenGamepad(ids[player]);
            tester_pads_[ids[player]] = pad;
        }
        if (player > 0 && !pad) {
            break;
        }
        std::string source;
        uint32_t buttons = 0;
        if (player == 0) {
            source = t.tester_keyboard;
            buttons |= keyboard_buttons();
        }
        if (pad) {
            const char* name = SDL_GetGamepadName(pad);
            source += (source.empty() ? "" : " + ") + std::string(name ? name : "?");
            buttons |= gamepad_buttons(pad);
        }
        if (player > 0) {
            inset_separator(width);
        }
        std::string label = format(t.tester_player, std::to_string(player + 1)) + "  ·  " + source;
        std::string held = held_names(buttons);
        ImGui::PushID(player);
        row(width, label.c_str(), held.empty() ? t.tester_idle : held.c_str(), 0.0f, 0.0f, [] {});
        ImGui::PopID();
    }
    SDL_free(ids);
    card.end();
    ui::gap(28);
}

void App::pad_map_section(float width) {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    ui::text(t.pad_map_title, Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(at + ImVec2(px(18), px(14)));
    ui::text(t.pad_map_detail, Font::Regular, ui::size::kDetail, p.secondary, width - px(36));
    ImGui::SetCursorScreenPos(ImVec2(at.x, ImGui::GetCursorScreenPos().y + px(4)));
    std::vector<std::string> labels = {t.pad_auto};
    for (int i = 0; i < wp::padmap::kPhysicalCount; i++) {
        labels.push_back(physical_label(i));
    }
    for (size_t action = 0; action < wp::padmap::kActionCount; action++) {
        inset_separator(width);
        std::string key = wp::padmap::setting_key(action);
        int index = wp::padmap::physical_from_name(value(key)) + 1;
        ImGui::PushID(static_cast<int>(action));
        row(width, wii_button_label(action), "", 260.0f, 36.0f, [&] {
            if (ui::dropdown("pad", labels, index, 260.0f)) {
                store(key, wp::padmap::physical_name(index - 1));
            }
        });
        ImGui::PopID();
    }
    card.end();
    ui::gap(28);
}
