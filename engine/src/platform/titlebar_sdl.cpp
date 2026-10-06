#include "wp/titlebar.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "wp/options_window.h"

namespace wp::titlebar {

namespace {

constexpr float kBarHeight = 32.0f;
constexpr float kButtonWidth = 46.0f;
constexpr float kResizeBorder = 6.0f;
constexpr int kButtons = 3;

struct State {
    std::mutex lock;
    SDL_Window* window = nullptr;
    std::string title;
    int hover = -1;
    int pressed = -1;
    bool dirty = true;
    int width = 0;
    uint64_t version = 0;
    options::Image image;
};

State g_state;

float scale_of(SDL_Window* window) {
    float scale = SDL_GetWindowDisplayScale(window);
    return scale >= 1.0f ? scale : 1.0f;
}

bool covered(SDL_Window* window) {
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

float density_of(SDL_Window* window) {
    float density = SDL_GetWindowPixelDensity(window);
    return density > 0.0f ? density : 1.0f;
}

int button_at(SDL_Window* window, float x, float y) {
    if (covered(window)) {
        return -1;
    }
    float density = density_of(window);
    float bar = static_cast<float>(height(window)) / density;
    float button = std::round(kButtonWidth * scale_of(window)) / density;
    int width = 0;
    int unused = 0;
    SDL_GetWindowSize(window, &width, &unused);
    if (y < 0.0f || y >= bar || x < static_cast<float>(width) - button * kButtons || x >= static_cast<float>(width)) {
        return -1;
    }
    return std::min(kButtons - 1, static_cast<int>((x - (static_cast<float>(width) - button * kButtons)) / button));
}

SDL_HitTestResult hit_test(SDL_Window* window, const SDL_Point* area, void*) {
    if (covered(window)) {
        return SDL_HITTEST_NORMAL;
    }
    int width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &width, &window_height);
    float bar = static_cast<float>(height(window)) / density_of(window);
    if (button_at(window, static_cast<float>(area->x), static_cast<float>(area->y)) >= 0) {
        return SDL_HITTEST_NORMAL;
    }
    if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)) {
        int edge = static_cast<int>(std::round(kResizeBorder * scale_of(window)));
        bool left = area->x < edge;
        bool right = area->x >= width - edge;
        bool top = area->y < edge;
        bool bottom = area->y >= window_height - edge;
        if (top && left) {
            return SDL_HITTEST_RESIZE_TOPLEFT;
        }
        if (top && right) {
            return SDL_HITTEST_RESIZE_TOPRIGHT;
        }
        if (bottom && left) {
            return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        }
        if (bottom && right) {
            return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        }
        if (top) {
            return SDL_HITTEST_RESIZE_TOP;
        }
        if (bottom) {
            return SDL_HITTEST_RESIZE_BOTTOM;
        }
        if (left) {
            return SDL_HITTEST_RESIZE_LEFT;
        }
        if (right) {
            return SDL_HITTEST_RESIZE_RIGHT;
        }
    }
    return static_cast<float>(area->y) < bar ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

void touch() {
    g_state.dirty = true;
}

void act(SDL_Window* window, int button) {
    if (button == 0) {
        SDL_MinimizeWindow(window);
    } else if (button == 1) {
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) {
            SDL_RestoreWindow(window);
        } else {
            SDL_MaximizeWindow(window);
        }
    } else {
        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        event.window.windowID = SDL_GetWindowID(window);
        SDL_PushEvent(&event);
    }
}

}

int height(SDL_Window* window) {
    if (!window || covered(window)) {
        return 0;
    }
    return static_cast<int>(std::lround(kBarHeight * scale_of(window)));
}

void install(SDL_Window* window) {
    {
        std::lock_guard<std::mutex> hold(g_state.lock);
        g_state.window = window;
        touch();
    }
    SDL_SetWindowHitTest(window, hit_test, nullptr);
}

void set_title(const std::string& title) {
    std::lock_guard<std::mutex> hold(g_state.lock);
    if (title != g_state.title) {
        g_state.title = title;
        touch();
    }
}

bool handle_event(SDL_Window* window, const SDL_Event& event) {
    std::lock_guard<std::mutex> hold(g_state.lock);
    switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION: {
        int index = button_at(window, event.motion.x, event.motion.y);
        if (index != g_state.hover) {
            g_state.hover = index;
            touch();
        }
        return false;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (event.button.button != SDL_BUTTON_LEFT) {
            return false;
        }
        int index = button_at(window, event.button.x, event.button.y);
        if (index < 0) {
            return false;
        }
        g_state.pressed = index;
        touch();
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (event.button.button != SDL_BUTTON_LEFT || g_state.pressed < 0) {
            return false;
        }
        int pressed = g_state.pressed;
        g_state.pressed = -1;
        touch();
        if (button_at(window, event.button.x, event.button.y) == pressed) {
            act(window, pressed);
        }
        return true;
    }
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        g_state.hover = -1;
        touch();
        return false;
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        touch();
        return false;
    default:
        return false;
    }
}

bool take(uint64_t& version, options::Image& image) {
    std::lock_guard<std::mutex> hold(g_state.lock);
    SDL_Window* window = g_state.window;
    if (!window) {
        return false;
    }
    int width = 0;
    int unused = 0;
    SDL_GetWindowSizeInPixels(window, &width, &unused);
    if (g_state.dirty || width != g_state.width) {
        g_state.dirty = false;
        g_state.width = width;
        bool maximized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
        bool focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        g_state.image = options::render_titlebar(width, height(window), g_state.title, g_state.hover, g_state.pressed, maximized, focused);
        g_state.version++;
    }
    if (g_state.version == version) {
        return false;
    }
    version = g_state.version;
    image = g_state.image;
    return !image.pixels.empty();
}

}
