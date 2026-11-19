#include "wp/input.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "wp/game.h"
#include "wp/gamepad.h"
#include "wp/gx.h"
#include "wp/keymap.h"
#include "wp/log.h"
#include "wp/modules.h"
#include "wp/settings.h"
#include "wp/video.h"
#include "wp/runtime.h"

namespace wp::input {

namespace {

constexpr auto kWheelSwing = std::chrono::milliseconds(120);
constexpr int64_t kNoticePulsePeriod = 500;
constexpr int64_t kNoticePulseLength = 100;
std::atomic<int64_t> g_wheel_up{0};
std::atomic<int64_t> g_wheel_down{0};
std::atomic<bool> g_blocked{false};
std::atomic<bool> g_release_wait[4] = {};

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool sideways_minigame() {
    static const settings::LiveFlag automatic("input.auto_grip", "WP_AUTO_ORIENTATION");
    if (!automatic()) {
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
    if (key == keymap::kWheelUp || key == keymap::kWheelDown) {
        int64_t last = (key == keymap::kWheelUp ? g_wheel_up : g_wheel_down).load();
        return now_ms() - last < kWheelSwing.count();
    }
    return video::key_down(key);
}

std::atomic<int64_t> g_sample_ns{0};
std::atomic<int64_t> g_sample_calls{0};

bool any_pressed(const std::vector<int>& keys) {
    for (int key : keys) {
        if (pressed(key)) {
            return true;
        }
    }
    return false;
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

const keymap::Bindings& bindings() {
    static std::mutex mutex;
    static std::atomic<const keymap::Bindings*> current{nullptr};
    static std::atomic<uint32_t> built{0};
    uint32_t revision = settings::revision();
    if (built.load() != revision || !current.load()) {
        std::lock_guard<std::mutex> lock(mutex);
        if (built.load() != revision || !current.load()) {
            std::vector<std::string> invalid;
            keymap::Bindings* result = new keymap::Bindings(keymap::load([](const std::string& key) { return settings::text(key.c_str(), nullptr); }, invalid));
            for (const std::string& entry : invalid) {
                std::fprintf(stderr, "settings: ignored key name %s", entry.c_str());
                std::fputc(10, stderr);
                log::write("input", "ignored key name %s", entry.c_str());
            }
            current.store(result);
            built.store(revision);
        }
    }
    return *current.load();
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
        bool toggle = any_pressed(bindings().of(keymap::Action::Grip));
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

uint32_t mouse_motion(MouseMotionState& state, bool active, long x, long y, int64_t now) {
    constexpr double kSlow = 0.15;
    constexpr double kFast = 2.0;
    constexpr int64_t kStep = 8;
    constexpr int64_t kHold = 120;
    if (!active) {
        state = MouseMotionState{};
        return 0;
    }
    if (!state.tracking) {
        state.tracking = true;
        state.x = x;
        state.y = y;
        state.time = now;
        return 0;
    }
    int64_t elapsed = now - state.time;
    if (elapsed >= kStep) {
        double vx = static_cast<double>(x - state.x) / static_cast<double>(elapsed);
        double vy = static_cast<double>(y - state.y) / static_cast<double>(elapsed);
        state.x = x;
        state.y = y;
        state.time = now;
        double ax = vx < 0.0 ? -vx : vx;
        double ay = vy < 0.0 ? -vy : vy;
        double speed = ax > ay ? ax : ay;
        uint32_t bits = 0;
        if (speed >= kFast) {
            bits = ay > ax ? (vy < 0.0 ? kMotionSwingUp : kMotionSwingDown) : kMotionShake;
        } else if (speed >= kSlow) {
            bits = ax >= ay ? (vx < 0.0 ? kMotionTiltLeft : kMotionTiltRight) : (vy < 0.0 ? kMotionTiltUp : kMotionTiltDown);
        }
        if (bits != 0) {
            state.held = bits;
            state.until = now + kHold;
        }
    }
    return now < state.until ? state.held : 0;
}

bool wakes_remote(uint32_t channel) {
    Sample current = sample(channel);
    if ((current.buttons & ~kMotionSideways) != 0) {
        return true;
    }
    static const settings::LiveFlag wake_on_mouse("input.wake_on_mouse", "WP_RECONNECT_ON_POINTER");
    bool pointer_wakes = wake_on_mouse();
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

Sample device_sample(uint32_t channel) {
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
        static const bool by_frames = std::getenv("WP_SCRIPT_CLOCK") && std::strcmp(std::getenv("WP_SCRIPT_CLOCK"), "frames") == 0;
        int elapsed = by_frames ? static_cast<int>(gx::frames_drawn() * 1000 / 60)
                                : static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - origin).count());
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
    void* window = video::window_handle();
    static const bool log_input = std::getenv("WP_LOG_INPUT") != nullptr;
    static bool logged_focus = false;
    bool focused = video::window_focused();
    if (log_input && channel == 0 && focused != logged_focus) {
        logged_focus = focused;
        std::fprintf(stderr, "input window=%p focused=%d", window, focused ? 1 : 0);
        std::fputc(10, stderr);
    }
    if (window != nullptr && !focused) {
        return result;
    }
    if (channel != 0) {
        return gamepad_sample(channel, sideways_grip(false));
    }
    if (window == nullptr) {
        return result;
    }
    static int64_t last_reload = 0;
    int64_t clock = now_ms();
    if (clock - last_reload >= 500) {
        last_reload = clock;
        settings::reload_if_changed();
    }
    const keymap::Bindings& keys = bindings();
    for (size_t i = 0; i < keymap::kActionCount; i++) {
        if (keymap::action(i).buttons != 0 && any_pressed(keys.codes[i])) {
            result.buttons |= keymap::action(i).buttons;
        }
    }
    bool sideways = sideways_grip(true);
    if (sideways) {
        uint32_t keys_held = result.buttons;
        result.buttons &= ~(kButtonUp | kButtonDown | kButtonLeft | kButtonRight);
        result.buttons |= ((keys_held & kButtonLeft) ? kButtonUp : 0) | ((keys_held & kButtonRight) ? kButtonDown : 0) | ((keys_held & kButtonUp) ? kButtonRight : 0) |
                          ((keys_held & kButtonDown) ? kButtonLeft : 0);
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
    static uint32_t logged_buttons = 0;
    if (log_input && result.buttons != logged_buttons) {
        logged_buttons = result.buttons;
        std::fprintf(stderr, "input buttons=%04x", result.buttons);
        std::fputc(10, stderr);
    }
    static long last_x = 0;
    static long last_y = 0;
    static int64_t last_mouse_move = -1000000;
    long cursor_x = 0;
    long cursor_y = 0;
    if (video::cursor_position(cursor_x, cursor_y)) {
        if (cursor_x != last_x || cursor_y != last_y) {
            last_x = cursor_x;
            last_y = cursor_y;
            last_mouse_move = now;
        }
    }
    static MouseMotionState mouse_state;
    result.buttons |= mouse_motion(mouse_state, any_pressed(keys.of(keymap::Action::MouseMotion)), cursor_x, cursor_y, now);
    bool mouse_recent = now - last_mouse_move < 1000;
    if ((mouse_recent || !pad.pointer_valid) && video::cursor_on_image(result.pointer_x, result.pointer_y)) {
        result.pointer_valid = true;
    } else if (pad.pointer_valid) {
        result.pointer_valid = true;
        result.pointer_x = pad.pointer_x;
        result.pointer_y = pad.pointer_y;
    }
    return result;
}

bool skipping_notice() {
    static const settings::LiveFlag setting("system.skip_notices", "WP_SKIP_NOTICES");
    static const bool scripted_input = std::getenv("WP_INPUT_SCRIPT") || std::getenv("WP_INPUT_BUTTONS");
    const char* module = game::description().notice_module;
    if (scripted_input || !setting() || !module || !module_loaded(module)) {
        return false;
    }
    return now_ms() % kNoticePulsePeriod < kNoticePulseLength;
}

void set_blocked(bool blocked) {
    if (!blocked) {
        for (std::atomic<bool>& wait : g_release_wait) {
            wait = true;
        }
    }
    g_blocked = blocked;
}

double take_sample_seconds(uint32_t& calls) {
    calls = static_cast<uint32_t>(g_sample_calls.exchange(0));
    return static_cast<double>(g_sample_ns.exchange(0)) / 1e9;
}

Sample sample(uint32_t channel) {
    auto started = std::chrono::steady_clock::now();
    Sample result = device_sample(channel);
    g_sample_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
    g_sample_calls++;
    uint32_t held = result.buttons & ~kMotionSideways;
    bool waiting = channel < 4 && g_release_wait[channel].load();
    if (g_blocked.load() || (waiting && held != 0)) {
        result.buttons &= kMotionSideways;
        return result;
    }
    if (waiting) {
        g_release_wait[channel] = false;
    }
    if (channel == 0 && skipping_notice()) {
        result.buttons |= kButtonA;
    }
    return result;
}

}
