#include "wp/input.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

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
};

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

bool connected(uint32_t channel) {
    return channel == 0;
}

Sample sample(uint32_t channel) {
    static const char* script_text = std::getenv("WP_INPUT_SCRIPT");
    if (script_text) {
        static const std::vector<ScriptEntry> script = parse_script(script_text);
        static const auto origin = std::chrono::steady_clock::now();
        int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - origin).count());
        return channel == 0 ? scripted(script, elapsed) : Sample{};
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
    if (channel != 0 || window == nullptr || GetForegroundWindow() != window) {
        return result;
    }
    for (const Binding& binding : kBindings) {
        if (pressed(binding.key)) {
            result.buttons |= binding.button;
        }
    }
    static uint32_t logged_buttons = 0;
    if (log_input && result.buttons != logged_buttons) {
        logged_buttons = result.buttons;
        std::fprintf(stderr, "input buttons=%04x", result.buttons);
        std::fputc(10, stderr);
    }
    POINT cursor;
    RECT client;
    if (GetCursorPos(&cursor) && ScreenToClient(window, &cursor) && GetClientRect(window, &client) &&
        video::image_point(cursor.x, cursor.y, client.right, client.bottom, result.pointer_x, result.pointer_y)) {
        result.pointer_valid = true;
    }
    return result;
}

}
