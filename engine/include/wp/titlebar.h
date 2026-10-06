#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "wp/options.h"

namespace wp::titlebar {

int height(SDL_Window* window);
void install(SDL_Window* window);
void set_title(const std::string& title);
bool handle_event(SDL_Window* window, const SDL_Event& event);
bool take(uint64_t& version, options::Image& image);

}
