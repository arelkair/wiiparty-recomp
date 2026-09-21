#include "wp/input.h"

#include <windows.h>

#include <cstdlib>

#include "wp/video.h"

namespace wp::input {

namespace {

struct Binding {
    int key;
    uint32_t button;
};

constexpr Binding kBindings[] = {
    {VK_LEFT, kButtonLeft},   {VK_RIGHT, kButtonRight}, {VK_DOWN, kButtonDown},        {VK_UP, kButtonUp},
    {VK_RETURN, kButtonA},    {VK_SPACE, kButtonA},     {VK_LBUTTON, kButtonA},        {VK_BACK, kButtonB},
    {VK_RBUTTON, kButtonB},   {'1', kButtonOne},        {'2', kButtonTwo},             {VK_OEM_PLUS, kButtonPlus},
    {VK_ADD, kButtonPlus},    {VK_OEM_MINUS, kButtonMinus}, {VK_SUBTRACT, kButtonMinus}, {'H', kButtonHome},
};

bool pressed(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

}

bool connected(uint32_t channel) {
    return channel == 0;
}

Sample sample(uint32_t channel) {
    Sample result;
    if (const char* forced = std::getenv("WP_INPUT_BUTTONS")) {
        static int frames = 0;
        if (frames++ > 150) {
            result.buttons = static_cast<uint32_t>(std::strtoul(forced, nullptr, 16));
        }
        return result;
    }
    HWND window = static_cast<HWND>(video::window_handle());
    if (channel != 0 || window == nullptr || GetForegroundWindow() != window) {
        return result;
    }
    for (const Binding& binding : kBindings) {
        if (pressed(binding.key)) {
            result.buttons |= binding.button;
        }
    }
    POINT cursor;
    RECT client;
    if (GetCursorPos(&cursor) && ScreenToClient(window, &cursor) && GetClientRect(window, &client) && client.right > 0 &&
        client.bottom > 0 && cursor.x >= 0 && cursor.y >= 0 && cursor.x < client.right && cursor.y < client.bottom) {
        result.pointer_valid = true;
        result.pointer_x = static_cast<float>(cursor.x) / client.right * 2.0f - 1.0f;
        result.pointer_y = static_cast<float>(cursor.y) / client.bottom * 2.0f - 1.0f;
    }
    return result;
}

}
