#pragma once

#include <chrono>
#include <cstdint>

namespace wp::vclock {

using Clock = std::chrono::steady_clock;

bool deterministic();
bool turbo();
Clock::time_point now();
int64_t elapsed_ns();
void advance(Clock::duration step);
void jump_to(Clock::time_point target);
void jump_to_ns(int64_t nanoseconds);
int64_t ns_of(Clock::time_point point);
int64_t now_ns();

}
