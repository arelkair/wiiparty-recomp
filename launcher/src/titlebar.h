#pragma once

#include <SDL3/SDL.h>

#include "imgui.h"

namespace titlebar {

constexpr float kHeight = 32.0f;

float height();
void install(SDL_Window* window);
void draw(SDL_Window* window, ImVec2 origin, float width);

}
