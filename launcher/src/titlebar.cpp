#include "titlebar.h"

#include <algorithm>
#include <cmath>

#include "ui.h"

namespace titlebar {

namespace {

constexpr float kButtonWidth = 46.0f;
constexpr float kResizeBorder = 6.0f;
constexpr int kButtons = 3;
constexpr ImU32 kCloseHover = IM_COL32(232, 17, 35, 255);
constexpr ImU32 kCloseGlyph = IM_COL32(255, 255, 255, 255);

SDL_HitTestResult hit_test(SDL_Window* window, const SDL_Point* area, void*) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    float bar = ui::px(kHeight);
    if (area->y < bar && area->x >= width - ui::px(kButtonWidth) * kButtons) {
        return SDL_HITTEST_NORMAL;
    }
    if (!(SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN))) {
        int edge = static_cast<int>(ui::px(kResizeBorder));
        bool left = area->x < edge;
        bool right = area->x >= width - edge;
        bool top = area->y < edge;
        bool bottom = area->y >= height - edge;
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
    return area->y < bar ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

void glyph(ImDrawList* list, int index, bool maximized, ImVec2 center, ImU32 color) {
    float thickness = std::max(1.0f, std::floor(ui::scale()));
    float half = std::round(ui::px(5));
    ImVec2 c(std::floor(center.x) + 0.5f, std::floor(center.y) + 0.5f);
    if (index == 0) {
        list->AddLine(ImVec2(c.x - half, c.y), ImVec2(c.x + half, c.y), color, thickness);
    } else if (index == 1 && !maximized) {
        list->AddRect(ImVec2(c.x - half, c.y - half), ImVec2(c.x + half, c.y + half), color, 0.0f, thickness);
    } else if (index == 1) {
        float step = std::round(ui::px(2));
        list->AddRect(ImVec2(c.x - half, c.y - half + step), ImVec2(c.x + half - step, c.y + half), color, 0.0f, thickness);
        list->AddLine(ImVec2(c.x - half + step, c.y - half), ImVec2(c.x + half, c.y - half), color, thickness);
        list->AddLine(ImVec2(c.x + half, c.y - half), ImVec2(c.x + half, c.y + half - step), color, thickness);
    } else {
        list->AddLine(ImVec2(c.x - half, c.y - half), ImVec2(c.x + half, c.y + half), color, thickness);
        list->AddLine(ImVec2(c.x - half, c.y + half), ImVec2(c.x + half, c.y - half), color, thickness);
    }
}

}

float height() {
    return ui::px(kHeight);
}

void install(SDL_Window* window) {
    SDL_SetWindowHitTest(window, hit_test, nullptr);
}

void draw(SDL_Window* window, ImVec2 origin, float width) {
    const ui::Palette& p = ui::palette();
    ImDrawList* list = ImGui::GetWindowDrawList();
    float bar = height();
    float button = ui::px(kButtonWidth);
    bool maximized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
    for (int i = 0; i < kButtons; i++) {
        ImVec2 min(origin.x + width - button * static_cast<float>(kButtons - i), origin.y);
        ImVec2 max(min.x + button, min.y + bar);
        ImGui::SetCursorScreenPos(min);
        ImGui::PushID(i);
        bool clicked = ImGui::InvisibleButton("##window", ImVec2(button, bar));
        bool hovered = ImGui::IsItemHovered();
        float t = ui::animate(ImGui::GetItemID(), hovered ? 1.0f : 0.0f, 24.0f);
        bool close = i == kButtons - 1;
        ImU32 fill = close ? ui::fade(kCloseHover, t) : ui::fade(p.control_hover, t);
        list->AddRectFilled(min, max, fill);
        ImU32 color = ui::mix(p.secondary, close ? kCloseGlyph : p.text, t);
        glyph(list, i, maximized, ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), color);
        ImGui::PopID();
        if (!clicked) {
            continue;
        }
        if (i == 0) {
            SDL_MinimizeWindow(window);
        } else if (i == 1) {
            if (maximized) {
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
    if (!maximized) {
        ImVec2 size = ImGui::GetMainViewport()->Size;
        ImGui::GetForegroundDrawList()->AddRect(origin, origin + size, p.separator);
    }
}

}
