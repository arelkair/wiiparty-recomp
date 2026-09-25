#include "wp/input.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "wp/game.h"
#include "wp/gamepad.h"
#include "wp/modules.h"
#include "wp/video.h"

namespace wp::input {

namespace {

struct Binding {
    int key;
    uint32_t button;
};

constexpr Binding kBindings[] = {
    {VK_LEFT, kButtonLeft},   {VK_RIGHT, kButtonRight}, {VK_DOWN, kButtonDown},        {VK_UP, kButtonUp},
    {'S', kButtonLeft},       {'W', kButtonRight},      {'D', kButtonDown},            {'A', kButtonUp},
    {VK_RETURN, kButtonA},    {VK_SPACE, kButtonA},     {VK_LBUTTON, kButtonA},        {VK_BACK, kButtonB},
    {VK_RBUTTON, kButtonB},   {'1', kButtonOne},        {'2', kButtonTwo},             {VK_OEM_PLUS, kButtonPlus},
    {VK_ADD, kButtonPlus},    {VK_OEM_MINUS, kButtonMinus}, {VK_SUBTRACT, kButtonMinus}, {'H', kButtonHome},
    {VK_MBUTTON, kMotionShake}, {VK_LSHIFT, kMotionShake}, {'T', kMotionSwingUp},        {'G', kMotionSwingDown},
    {'Q', kMotionTiltLeft},   {'E', kMotionTiltRight},  {'R', kMotionTiltUp},          {'F', kMotionTiltDown},
};

constexpr auto kWheelSwing = std::chrono::milliseconds(120);
std::atomic<int64_t> g_wheel_up{0};
std::atomic<int64_t> g_wheel_down{0};

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool sideways_minigame() {
    static const bool automatic = [] {
        const char* setting = std::getenv("WP_AUTO_ORIENTATION");
        return !setting || std::atoi(setting) != 0;
    }();
    if (!automatic) {
        return false;
    }
    static int64_t next = 0;
    static bool cached = false;
    int64_t now = now_ms();
    if (now >= next) {
        next = now + 100;
        cached = false;
        const game::Description& game = game::description();
        for (size_t i = 0; i < game.sideways_module_count; i++) {
            if (module_loaded(game.sideways_modules[i])) {
                cached = true;
                break;
            }
        }
    }
    return cached;
}

bool pressed(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

struct ScriptEntry {
    int from = 0;
    int to = 0;
    uint32_t buttons = 0;
    bool pointer = false;
    float x = 0.0f;
    float y = 0.0f;
};

std::vector<ScriptEntry> parse_script(const char* text) {
    std::vector<ScriptEntry> entries;
    std::stringstream lines(text);
    std::string line;
    while (std::getline(lines, line, ';')) {
        std::stringstream fields(line);
        std::string field;
        std::vector<std::string> parts;
        while (std::getline(fields, field, ',')) {
            parts.push_back(field);
        }
        if (parts.size() < 3) {
            continue;
        }
        ScriptEntry entry;
        entry.from = std::atoi(parts[0].c_str());
        entry.to = std::atoi(parts[1].c_str());
        entry.buttons = static_cast<uint32_t>(std::strtoul(parts[2].c_str(), nullptr, 16));
        if (parts.size() >= 5) {
            entry.pointer = true;
            entry.x = static_cast<float>(std::atof(parts[3].c_str()));
            entry.y = static_cast<float>(std::atof(parts[4].c_str()));
        }
        entries.push_back(entry);
    }
    return entries;
}

Sample scripted(const std::vector<ScriptEntry>& script, int milliseconds) {
    Sample result;
    for (const ScriptEntry& entry : script) {
        if (milliseconds >= entry.from && milliseconds < entry.to) {
            result.buttons |= entry.buttons;
            if (entry.pointer) {
                result.pointer_valid = true;
                result.pointer_x = entry.x;
                result.pointer_y = entry.y;
            }
        }
    }
    return result;
}

}

void note_wheel(int delta) {
    if (delta > 0) {
        g_wheel_up = now_ms();
    } else if (delta < 0) {
        g_wheel_down = now_ms();
    }
}

namespace {

bool sideways_grip(bool read_toggle) {
    static bool flipped = false;
    static bool toggle_held = false;
    if (read_toggle) {
        bool toggle = pressed(VK_TAB);
        if (toggle && !toggle_held) {
            flipped = !flipped;
        }
        toggle_held = toggle;
    }
    return sideways_minigame() != flipped;
}

Sample gamepad_sample(uint32_t channel, bool sideways) {
    Sample result;
    if (sideways) {
        result.buttons |= kMotionSideways;
    }
    gamepad::State pad = gamepad::poll(channel, sideways);
    result.buttons |= pad.buttons;
    result.pointer_valid = pad.pointer_valid;
    result.pointer_x = pad.pointer_x;
    result.pointer_y = pad.pointer_y;
    result.motion_valid = pad.motion_valid;
    for (int i = 0; i < 3; i++) {
        result.accel[i] = pad.accel[i];
    }
    return result;
}

}

bool wakes_remote(uint32_t channel) {
    Sample current = sample(channel);
    if ((current.buttons & ~kMotionSideways) != 0) {
        return true;
    }
    static const bool pointer_wakes = [] {
        const char* setting = std::getenv("WP_RECONNECT_ON_POINTER");
        return !setting || std::atoi(setting) != 0;
    }();
    static bool had_pointer = false;
    static float last_x = 0.0f;
    static float last_y = 0.0f;
    bool moved = pointer_wakes && channel == 0 && current.pointer_valid && had_pointer && (current.pointer_x != last_x || current.pointer_y != last_y);
    if (channel == 0) {
        had_pointer = current.pointer_valid;
        last_x = current.pointer_x;
        last_y = current.pointer_y;
    }
    return moved;
}

bool connected(uint32_t channel) {
    return channel == 0 || gamepad::connected(channel);
}

Sample sample(uint32_t channel) {
    static const char* script_text = std::getenv("WP_INPUT_SCRIPT");
    if (script_text) {
        static const std::vector<ScriptEntry> script = [] {
            std::string text = script_text;
            if (!text.empty() && text[0] == '@') {
                std::ifstream file(text.substr(1), std::ios::binary);
                std::stringstream contents;
                contents << file.rdbuf();
                text = contents.str();
            }
            return parse_script(text.c_str());
        }();
        static const auto origin = std::chrono::steady_clock::now();
        int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - origin).count());
        if (channel != 0) {
            return gamepad_sample(channel, sideways_grip(false));
        }
        return scripted(script, elapsed);
    }
    Sample result;
    if (const char* forced = std::getenv("WP_INPUT_BUTTONS")) {
        static int frames = 0;
        if (frames++ > 150) {
            result.buttons = static_cast<uint32_t>(std::strtoul(forced, nullptr, 16));
        }
        return result;
    }
    HWND window = static_cast<HWND>(video::window_handle());
    static const bool log_input = std::getenv("WP_LOG_INPUT") != nullptr;
    static bool logged_focus = false;
    static HWND logged_foreground = nullptr;
    bool focused = window != nullptr && GetForegroundWindow() == window;
    if (log_input && channel == 0 && (focused != logged_focus || GetForegroundWindow() != logged_foreground)) {
        logged_focus = focused;
        logged_foreground = GetForegroundWindow();
        std::fprintf(stderr, "input window=%p focused=%d foreground=%p", static_cast<void*>(window), focused ? 1 : 0,
                     static_cast<void*>(GetForegroundWindow()));
        std::fputc(10, stderr);
    }
    if (window != nullptr && GetForegroundWindow() != window) {
        return result;
    }
    if (channel != 0) {
        return gamepad_sample(channel, sideways_grip(false));
    }
    if (window == nullptr) {
        return result;
    }
    for (const Binding& binding : kBindings) {
        if (pressed(binding.key)) {
            result.buttons |= binding.button;
        }
    }
    bool sideways = sideways_grip(true);
    if (sideways) {
        result.buttons |= kMotionSideways;
    }
    gamepad::State pad = gamepad::poll(channel, sideways);
    result.buttons |= pad.buttons;
    if (pad.motion_valid) {
        result.motion_valid = true;
        for (int i = 0; i < 3; i++) {
            result.accel[i] = pad.accel[i];
        }
    }
    int64_t now = now_ms();
    if (now - g_wheel_up.load() < kWheelSwing.count()) {
        result.buttons |= kMotionSwingUp;
    }
    if (now - g_wheel_down.load() < kWheelSwing.count()) {
        result.buttons |= kMotionSwingDown;
    }
    static uint32_t logged_buttons = 0;
    if (log_input && result.buttons != logged_buttons) {
        logged_buttons = result.buttons;
        std::fprintf(stderr, "input buttons=%04x", result.buttons);
        std::fputc(10, stderr);
    }
    POINT cursor;
    RECT client;
    static POINT last_cursor{};
    static int64_t last_mouse_move = -1000000;
    if (GetCursorPos(&cursor)) {
        if (cursor.x != last_cursor.x || cursor.y != last_cursor.y) {
            last_cursor = cursor;
            last_mouse_move = now;
        }
    }
    bool mouse_recent = now - last_mouse_move < 1000;
    if ((mouse_recent || !pad.pointer_valid) && ScreenToClient(window, &cursor) && GetClientRect(window, &client) &&
        video::image_point(cursor.x, cursor.y, client.right, client.bottom, result.pointer_x, result.pointer_y)) {
        result.pointer_valid = true;
    } else if (pad.pointer_valid) {
        result.pointer_valid = true;
        result.pointer_x = pad.pointer_x;
        result.pointer_y = pad.pointer_y;
    }
    return result;
}

}
