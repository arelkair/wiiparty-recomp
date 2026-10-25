#include "app.h"

#include <algorithm>
#include <cmath>

#include "keys.h"
#include "layout.h"
#include "texts.h"
#include "wp/input.h"
#include "wp/keymap.h"
#include "wp/padmap.h"
#include "wp/ui_text.h"

namespace {

constexpr float kStickThreshold = 0.5f;
constexpr float kTriggerThreshold = 0.5f;

enum class PadStyle { Xbox, Nintendo, PlayStation };

PadStyle pad_style() {
    int count = 0;
    SDL_JoystickID* ids = gamepads(count);
    PadStyle style = PadStyle::Xbox;
    if (count > 0) {
        switch (SDL_GetGamepadTypeForID(ids[0])) {
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            style = PadStyle::Nintendo;
            break;
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5:
            style = PadStyle::PlayStation;
            break;
        default:
            break;
        }
    }
    SDL_free(ids);
    return style;
}

std::string face_name(int physical, PadStyle style) {
    static const char* xbox[] = {"A", "B", "X", "Y"};
    static const char* nintendo[] = {"B", "A", "Y", "X"};
    static const char* playstation[] = {"pad.cross", "pad.circle", "pad.square", "pad.triangle"};
    if (style == PadStyle::PlayStation) {
        return wp::ui::label(playstation[physical]);
    }
    return style == PadStyle::Nintendo ? nintendo[physical] : xbox[physical];
}

std::string physical_label(int physical, PadStyle style) {
    const Texts& t = texts();
    const char* labels[] = {t.pad_south,      t.pad_east,          t.pad_west,           t.pad_north,    t.pad_back,      t.pad_guide,
                            t.pad_start,      t.pad_left_stick,    t.pad_right_stick,    t.pad_left_shoulder, t.pad_right_shoulder,
                            t.pad_dpad_up,    t.pad_dpad_down,     t.pad_dpad_left,      t.pad_dpad_right,    t.pad_left_trigger, t.pad_right_trigger};
    if (physical > wp::padmap::kRightTrigger && physical < wp::padmap::kPhysicalCount) {
        std::string key = std::string("pad.") + wp::padmap::physical_name(physical);
        return wp::ui::label(key.c_str());
    }
    if (physical >= wp::padmap::kSouth && physical <= wp::padmap::kNorth) {
        return face_name(physical, style) + " – " + labels[physical];
    }
    return physical >= 0 && physical <= wp::padmap::kRightTrigger ? labels[physical] : t.pad_auto;
}

const char* wii_button_label(size_t action) {
    const char* labels[] = {"A", "B", "1", "2", "+", "−", "HOME"};
    if (action < wp::padmap::kButtonActionCount) {
        return labels[action];
    }
    return action < wp::padmap::kActionCount ? action_name(wp::padmap::action_name(action)) : "";
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
    float right_x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
    float right_y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
    pressed.held[wp::padmap::kRightStickUp] = right_y < -kStickThreshold;
    pressed.held[wp::padmap::kRightStickDown] = right_y > kStickThreshold;
    pressed.held[wp::padmap::kRightStickLeft] = right_x < -kStickThreshold;
    pressed.held[wp::padmap::kRightStickRight] = right_x > kStickThreshold;
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

namespace {

void draw_face_symbol(ImDrawList* list, int physical, ImVec2 center, float radius, ImU32 color, float thickness) {
    float r = radius * 0.5f;
    switch (physical) {
    case wp::padmap::kSouth:
        list->AddLine(center + ImVec2(-r, -r), center + ImVec2(r, r), color, thickness);
        list->AddLine(center + ImVec2(-r, r), center + ImVec2(r, -r), color, thickness);
        break;
    case wp::padmap::kEast:
        list->AddCircle(center, r * 1.1f, color, 24, thickness);
        break;
    case wp::padmap::kWest:
        list->AddRect(center + ImVec2(-r, -r), center + ImVec2(r, r), color, 0.0f, thickness);
        break;
    default:
        list->AddTriangle(center + ImVec2(0, -r * 1.15f), center + ImVec2(r * 1.1f, r * 0.85f), center + ImVec2(-r * 1.1f, r * 0.85f), color, thickness);
        break;
    }
}

}

void App::pad_view_section(float width) {
    const ui::Palette& p = ui::palette();
    ui::text(wp::ui::label("pad.view_title"), Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    float height = px(300);
    ImVec2 at = ImGui::GetCursorScreenPos();
    ImDrawList* list = ImGui::GetWindowDrawList();
    SDL_Gamepad* pad = nullptr;
    int count = 0;
    SDL_JoystickID* ids = gamepads(count);
    if (count > 0) {
        auto found = tester_pads_.find(ids[0]);
        if (found != tester_pads_.end()) {
            pad = found->second;
        }
    }
    SDL_free(ids);
    PadStyle style = pad_style();
    bool nintendo = style == PadStyle::Nintendo;
    bool playstation = style == PadStyle::PlayStation;
    ImVec2 c(at.x + width * 0.5f, at.y + height * 0.5f + px(14));
    auto pt = [&](float x, float y) { return c + ImVec2(px(x), px(y)); };
    auto down = [&](SDL_GamepadButton button) { return pad && SDL_GetGamepadButton(pad, button); };
    auto axis = [&](SDL_GamepadAxis which) { return pad ? SDL_GetGamepadAxis(pad, which) / 32767.0f : 0.0f; };
    ImU32 body = p.control;
    ImU32 edge = p.tertiary;
    ImU32 idle = p.control_active;
    ImU32 lit = p.accent;

    struct Curve {
        float c1x, c1y, c2x, c2y, x, y;
    };
    const Curve curves[5] = {{100, -58, 125, -72, 140, -60}, {158, -48, 172, 10, 176, 55}, {180, 95, 160, 112, 140, 108}, {118, 104, 108, 70, 92, 50}, {80, 36, 60, 34, 40, 34}};
    const ImVec2 start(70, -58);
    auto outline = [&]() {
        list->PathClear();
        list->PathLineTo(pt(0, -58));
        list->PathLineTo(pt(start.x, start.y));
        for (const Curve& k : curves) {
            list->PathBezierCubicCurveTo(pt(k.c1x, k.c1y), pt(k.c2x, k.c2y), pt(k.x, k.y));
        }
        list->PathLineTo(pt(0, 34));
        float from_x = 40;
        float from_y = 34;
        for (int i = 4; i >= 0; i--) {
            const Curve& k = curves[i];
            float to_x = i == 0 ? start.x : curves[i - 1].x;
            float to_y = i == 0 ? start.y : curves[i - 1].y;
            list->PathBezierCubicCurveTo(pt(-k.c2x, k.c2y), pt(-k.c1x, k.c1y), pt(-to_x, to_y));
            from_x = to_x;
            from_y = to_y;
        }
        (void)from_x;
        (void)from_y;
        list->PathLineTo(pt(0, -58));
    };
    outline();
    list->PathFillConcave(body);
    outline();
    list->PathStroke(edge, px(1.5f), ImDrawFlags_Closed);

    const char* shoulder_names[2] = {nintendo ? "L" : playstation ? "L1" : "LB", nintendo ? "R" : playstation ? "R1" : "RB"};
    const char* trigger_names[2] = {nintendo ? "ZL" : playstation ? "L2" : "LT", nintendo ? "ZR" : playstation ? "R2" : "RT"};
    float triggers[2] = {axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)};
    bool shoulders[2] = {down(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER), down(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)};
    for (int side = 0; side < 2; side++) {
        float x0 = side == 0 ? -150.0f : 84.0f;
        list->AddRectFilled(pt(x0, -86), pt(x0 + 66, -70), shoulders[side] ? lit : idle, px(8));
        list->AddRect(pt(x0, -86), pt(x0 + 66, -70), edge, px(8), px(1));
        list->AddRectFilled(pt(x0 + 6, -108), pt(x0 + 60, -94), idle, px(7));
        if (triggers[side] > 0.02f) {
            list->AddRectFilled(pt(x0 + 6, -108), pt(x0 + 6 + 54 * std::min(triggers[side], 1.0f), -94), lit, px(7));
        }
        ImVec2 label = ImGui::CalcTextSize(shoulder_names[side]);
        list->AddText(pt(x0 + 33, -78) - label * 0.5f, shoulders[side] ? p.primary_text : p.secondary, shoulder_names[side]);
        ImVec2 trigger = ImGui::CalcTextSize(trigger_names[side]);
        list->AddText(pt(x0 + 33, -101) - trigger * 0.5f, triggers[side] > 0.5f ? p.primary_text : p.secondary, trigger_names[side]);
    }

    struct Stick {
        float x;
        float y;
        SDL_GamepadAxis horizontal;
        SDL_GamepadAxis vertical;
        SDL_GamepadButton press;
    };
    const float left_x = -100;
    const float left_y = playstation ? 24.0f : -18.0f;
    const float dpad_x = playstation ? -100.0f : -55.0f;
    const float dpad_y = playstation ? -18.0f : 24.0f;
    const float left_stick_x = playstation ? -52.0f : left_x;
    const Stick sticks[2] = {{left_stick_x, left_y, SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_BUTTON_LEFT_STICK},
                             {52, 24, SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY, SDL_GAMEPAD_BUTTON_RIGHT_STICK}};
    for (const Stick& stick : sticks) {
        bool pressed = down(stick.press);
        list->AddCircleFilled(pt(stick.x, stick.y), px(25), idle, 40);
        list->AddCircle(pt(stick.x, stick.y), px(25), edge, 40, px(1.2f));
        ImVec2 knob = pt(stick.x + axis(stick.horizontal) * 14, stick.y + axis(stick.vertical) * 14);
        list->AddCircleFilled(knob, px(15), pressed ? lit : p.tertiary, 28);
        list->AddCircle(knob, px(15), edge, 28, px(1));
    }

    const SDL_GamepadButton arms[4] = {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    const float arm_x[4] = {0, 0, -17, 17};
    const float arm_y[4] = {-17, 17, 0, 0};
    list->AddRectFilled(pt(dpad_x - 12, dpad_y - 12), pt(dpad_x + 12, dpad_y + 12), idle);
    for (int i = 0; i < 4; i++) {
        float half_w = i < 2 ? 12.0f : 11.0f;
        float half_h = i < 2 ? 11.0f : 12.0f;
        ImVec2 a = pt(dpad_x + arm_x[i] - half_w, dpad_y + arm_y[i] - half_h);
        ImVec2 b = pt(dpad_x + arm_x[i] + half_w, dpad_y + arm_y[i] + half_h);
        list->AddRectFilled(a, b, down(arms[i]) ? lit : idle, px(4));
        list->AddRect(a, b, edge, px(4), px(1));
    }

    const SDL_GamepadButton faces[4] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH};
    const float face_x[4] = {100, 122, 78, 100};
    const float face_y[4] = {4, -18, -18, -40};
    for (int i = 0; i < 4; i++) {
        bool pressed = down(faces[i]);
        ImVec2 center = pt(face_x[i], face_y[i]);
        list->AddCircleFilled(center, px(13), pressed ? lit : idle, 28);
        list->AddCircle(center, px(13), edge, 28, px(1));
        ImU32 ink = pressed ? p.primary_text : p.secondary;
        if (playstation) {
            draw_face_symbol(list, i, center, px(26), ink, px(1.8f));
        } else {
            std::string name = face_name(i, style);
            ImVec2 size = ImGui::CalcTextSize(name.c_str());
            list->AddText(center - size * 0.5f, ink, name.c_str());
        }
    }

    const Stick middle[3] = {{-20, -32, SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_BUTTON_BACK},
                             {20, -32, SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_BUTTON_START},
                             {0, -14, SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_BUTTON_GUIDE}};
    for (const Stick& button : middle) {
        bool pressed = down(button.press);
        list->AddCircleFilled(pt(button.x, button.y), px(7), pressed ? lit : idle, 16);
        list->AddCircle(pt(button.x, button.y), px(7), edge, 16, px(1));
    }
    if (!pad) {
        const char* note = wp::ui::label("pad.view_none");
        ImVec2 size = ImGui::CalcTextSize(note);
        list->AddText(ImVec2(c.x - size.x * 0.5f, at.y + height - px(24)), p.secondary, note);
    }
    ImGui::Dummy(ImVec2(width, height));
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
    ImGui::SetCursorScreenPos(ImVec2(at.x + px(18), ImGui::GetCursorScreenPos().y + px(10)));
    ui::text(wp::ui::label("pad.motion_help"), Font::Regular, ui::size::kDetail, p.secondary, width - px(36));
    ImGui::SetCursorScreenPos(ImVec2(at.x, ImGui::GetCursorScreenPos().y + px(14)));
    std::vector<std::string> labels = {t.pad_auto};
    PadStyle style = pad_style();
    for (int i = 0; i < wp::padmap::kPhysicalCount; i++) {
        labels.push_back(physical_label(i, style));
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
