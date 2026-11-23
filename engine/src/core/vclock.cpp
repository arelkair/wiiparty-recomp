#include "wp/vclock.h"

#include <atomic>
#include <cstdlib>

namespace wp::vclock {

namespace {

const Clock::time_point& origin() {
    static const Clock::time_point value = Clock::now();
    return value;
}

std::atomic<int64_t> g_virtual_ns{0};

}

bool deterministic() {
    static const bool value = std::getenv("WP_DETERMINISTIC") != nullptr;
    return value;
}

bool turbo() {
    static const bool value = std::getenv("WP_DETERMINISTIC_TURBO") != nullptr;
    return value;
}

Clock::time_point now() {
    if (!deterministic()) {
        return Clock::now();
    }
    return origin() + std::chrono::nanoseconds(g_virtual_ns.load(std::memory_order_relaxed));
}

int64_t elapsed_ns() {
    if (!deterministic()) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - origin()).count();
    }
    return g_virtual_ns.load(std::memory_order_relaxed);
}

void advance(Clock::duration step) {
    g_virtual_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(step).count(), std::memory_order_relaxed);
}

void jump_to(Clock::time_point target) {
    int64_t wanted = std::chrono::duration_cast<std::chrono::nanoseconds>(target - origin()).count();
    int64_t current = g_virtual_ns.load(std::memory_order_relaxed);
    if (wanted > current) {
        g_virtual_ns.store(wanted, std::memory_order_relaxed);
    }
}

}
