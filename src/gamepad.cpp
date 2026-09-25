#include "wp/gamepad.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <thread>

#include "wp/input.h"
#include "wp/log.h"

namespace wp::gamepad {

namespace {

constexpr float kGravity = 9.80665f;
constexpr float kStickThreshold = 0.5f;
constexpr float kTriggerThreshold = 0.5f;
constexpr float kPointerPerRadian = 2.3f;
constexpr float kStickPointerSpeed = 1.6f;
constexpr float kPointerLimit = 1.3f;
constexpr auto kPollInterval = std::chrono::milliseconds(4);

struct Snapshot {
    bool connected = false;
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT] = {};
    float left_x = 0.0f;
    float left_y = 0.0f;
    float right_trigger = 0.0f;
    bool has_accel = false;
    float accel[3] = {0.0f, 0.0f, 0.0f};
    bool pointer_valid = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
};

std::mutex g_mutex;
Snapshot g_snapshot;
std::once_flag g_started;

float axis(SDL_Gamepad* pad, SDL_GamepadAxis which) {
    return static_cast<float>(SDL_GetGamepadAxis(pad, which)) / 32767.0f;
}

void open_first(SDL_Gamepad*& pad, bool& has_gyro, bool& has_accel) {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids && count > 0) {
        pad = SDL_OpenGamepad(ids[0]);
        if (pad) {
            has_gyro = SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO) && SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_GYRO, true);
            has_accel = SDL_GamepadHasSensor(pad, SDL_SENSOR_ACCEL) && SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_ACCEL, true);
            const char* name = SDL_GetGamepadName(pad);
            log::write("input", "gamepad %s connected, gyroscope %s, accelerometer %s", name ? name : "?", has_gyro ? "yes" : "no",
                       has_accel ? "yes" : "no");
        }
    }
    SDL_free(ids);
}

void worker() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        log::write("input", "SDL gamepad support unavailable: %s", SDL_GetError());
        return;
    }
    SDL_Gamepad* pad = nullptr;
    bool has_gyro = false;
    bool has_accel = false;
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
    bool pointer_active = false;
    bool recenter_held = false;
    auto previous = std::chrono::steady_clock::now();
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_GAMEPAD_REMOVED && pad && event.gdevice.which == SDL_GetGamepadID(pad)) {
                SDL_CloseGamepad(pad);
                pad = nullptr;
                log::write("input", "gamepad disconnected");
            }
        }
        if (!pad) {
            open_first(pad, has_gyro, has_accel);
            pointer_active = has_gyro;
            pointer_x = 0.0f;
            pointer_y = 0.0f;
        }
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;
        Snapshot snapshot;
        if (pad) {
            snapshot.connected = true;
            for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++) {
                snapshot.buttons[i] = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(i));
            }
            snapshot.left_x = axis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            snapshot.left_y = axis(pad, SDL_GAMEPAD_AXIS_LEFTY);
            snapshot.right_trigger = axis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            float right_x = axis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
            float right_y = axis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
            if (std::fabs(right_x) > 0.15f || std::fabs(right_y) > 0.15f) {
                pointer_active = true;
                pointer_x += right_x * kStickPointerSpeed * dt;
                pointer_y += right_y * kStickPointerSpeed * dt;
            }
            if (has_gyro) {
                float gyro[3] = {};
                if (SDL_GetGamepadSensorData(pad, SDL_SENSOR_GYRO, gyro, 3)) {
                    pointer_x -= gyro[1] * dt * kPointerPerRadian;
                    pointer_y -= gyro[0] * dt * kPointerPerRadian;
                }
            }
            bool recenter = snapshot.buttons[SDL_GAMEPAD_BUTTON_RIGHT_STICK];
            if (recenter && !recenter_held) {
                pointer_x = 0.0f;
                pointer_y = 0.0f;
                pointer_active = true;
            }
            recenter_held = recenter;
            pointer_x = std::clamp(pointer_x, -kPointerLimit, kPointerLimit);
            pointer_y = std::clamp(pointer_y, -kPointerLimit, kPointerLimit);
            snapshot.pointer_valid = pointer_active && std::fabs(pointer_x) < 1.0f && std::fabs(pointer_y) < 1.0f;
            snapshot.pointer_x = pointer_x;
            snapshot.pointer_y = pointer_y;
            if (has_accel) {
                snapshot.has_accel = SDL_GetGamepadSensorData(pad, SDL_SENSOR_ACCEL, snapshot.accel, 3);
            }
        }
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_snapshot = snapshot;
        }
        std::this_thread::sleep_for(kPollInterval);
    }
}

}

State poll(bool sideways) {
    std::call_once(g_started, [] { std::thread(worker).detach(); });
    Snapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        snapshot = g_snapshot;
    }
    State state;
    if (!snapshot.connected) {
        return state;
    }
    state.connected = true;
    auto held = [&](SDL_GamepadButton button) { return snapshot.buttons[button]; };
    uint32_t& b = state.buttons;
    if (held(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
        b |= input::kButtonA;
    }
    if (snapshot.right_trigger > kTriggerThreshold) {
        b |= input::kButtonB;
    }
    if (sideways) {
        if (held(SDL_GAMEPAD_BUTTON_SOUTH)) {
            b |= input::kButtonTwo;
        }
        if (held(SDL_GAMEPAD_BUTTON_WEST) || held(SDL_GAMEPAD_BUTTON_EAST)) {
            b |= input::kButtonOne;
        }
        if (held(SDL_GAMEPAD_BUTTON_NORTH)) {
            b |= input::kButtonA;
        }
    } else {
        if (held(SDL_GAMEPAD_BUTTON_SOUTH)) {
            b |= input::kButtonA;
        }
        if (held(SDL_GAMEPAD_BUTTON_EAST)) {
            b |= input::kButtonB;
        }
        if (held(SDL_GAMEPAD_BUTTON_WEST)) {
            b |= input::kButtonOne;
        }
        if (held(SDL_GAMEPAD_BUTTON_NORTH)) {
            b |= input::kButtonTwo;
        }
    }
    if (held(SDL_GAMEPAD_BUTTON_BACK)) {
        b |= input::kButtonMinus;
    }
    if (held(SDL_GAMEPAD_BUTTON_START)) {
        b |= input::kButtonPlus;
    }
    if (held(SDL_GAMEPAD_BUTTON_GUIDE)) {
        b |= input::kButtonHome;
    }
    bool left = held(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || snapshot.left_x < -kStickThreshold;
    bool right = held(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || snapshot.left_x > kStickThreshold;
    bool up = held(SDL_GAMEPAD_BUTTON_DPAD_UP) || snapshot.left_y < -kStickThreshold;
    bool down = held(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || snapshot.left_y > kStickThreshold;
    if (sideways) {
        b |= (left ? input::kButtonUp : 0) | (right ? input::kButtonDown : 0) | (up ? input::kButtonRight : 0) | (down ? input::kButtonLeft : 0);
    } else {
        b |= (left ? input::kButtonLeft : 0) | (right ? input::kButtonRight : 0) | (up ? input::kButtonUp : 0) | (down ? input::kButtonDown : 0);
    }
    state.pointer_valid = snapshot.pointer_valid;
    state.pointer_x = snapshot.pointer_x;
    state.pointer_y = snapshot.pointer_y;
    if (snapshot.has_accel) {
        float x = snapshot.accel[0] / kGravity;
        float y = snapshot.accel[1] / kGravity;
        float z = snapshot.accel[2] / kGravity;
        state.motion_valid = true;
        if (sideways) {
            state.accel[0] = z;
            state.accel[1] = -x;
        } else {
            state.accel[0] = -x;
            state.accel[1] = -z;
        }
        state.accel[2] = y;
    }
    return state;
}

}
