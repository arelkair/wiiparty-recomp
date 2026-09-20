#include "wp/audio.h"

#include <chrono>
#include <cstdint>

#include "wp/memory.h"

namespace wp::audio {

namespace {

constexpr uint32_t kControl = 0xCD006C00;
constexpr uint32_t kSampleCounter = 0xCD006C08;
constexpr uint32_t kPlaying = 0x01;
constexpr uint32_t kSampleRate48k = 0x02;
constexpr uint32_t kCounterReset = 0x20;
constexpr double kRate32k = 32000.0;
constexpr double kRate48k = 48000.0;

using Clock = std::chrono::steady_clock;

Clock::time_point g_last = Clock::now();
double g_samples = 0.0;

}

void update() {
    Clock::time_point now = Clock::now();
    double elapsed = std::chrono::duration<double>(now - g_last).count();
    g_last = now;
    uint32_t control = rd32(kControl);
    if (control & kCounterReset) {
        g_samples = 0.0;
        wr32(kControl, control & ~kCounterReset);
        control &= ~kCounterReset;
    }
    if (control & kPlaying) {
        g_samples += elapsed * ((control & kSampleRate48k) ? kRate48k : kRate32k);
    }
    wr32(kSampleCounter, static_cast<uint32_t>(g_samples) & 0x7FFFFFFF);
}

}
