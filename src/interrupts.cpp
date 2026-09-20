#include <chrono>

#include "wp/audio.h"
#include "wp/cpu.h"
#include "wp/hle.h"
#include "wp/memory.h"
#include "wp/threads.h"
#include "wp/video.h"

namespace wp {

uint32_t g_poll_counter = 0;

namespace {

constexpr uint32_t kMsrExternalInterrupt = 0x8000;
constexpr uint32_t kInterruptTable = 0x80003040;
constexpr uint32_t kVideoInterrupt = 24;
constexpr uint32_t kVideoInterruptRegisters[] = {0xCC002030, 0xCC002034};
constexpr uint16_t kVideoInterruptEnable = 0x1000;
constexpr uint16_t kVideoInterruptFlag = 0x8000;
constexpr uint32_t kCurrentContext = 0x800000D4;
constexpr uint32_t kDisplayConfig = 0xCC002002;
constexpr uint16_t kFormatMask = 0x0300;
constexpr uint16_t kFormatPal = 0x0100;
constexpr std::chrono::microseconds kPalPeriod(20000);
constexpr std::chrono::microseconds kNtscPeriod(16683);

using Clock = std::chrono::steady_clock;

std::chrono::microseconds retrace_period() {
    return (rd16(kDisplayConfig) & kFormatMask) == kFormatPal ? kPalPeriod : kNtscPeriod;
}

Clock::time_point g_next_retrace = Clock::now() + kPalPeriod;
bool g_in_interrupt = false;

bool arm_video_interrupt() {
    bool armed = false;
    for (uint32_t address : kVideoInterruptRegisters) {
        uint16_t value = rd16(address);
        if (value & kVideoInterruptEnable) {
            wr16(address, value | kVideoInterruptFlag);
            armed = true;
        }
    }
    return armed;
}

void deliver_ipc_interrupt(Cpu& c) {
    Cpu saved = c;
    g_in_interrupt = true;
    c.msr &= ~kMsrExternalInterrupt;
    ipc_deliver(c);
    c = saved;
    g_in_interrupt = false;
    c.r[3] = 0;
    call(c, symbol_address("OSSelectThread"));
    c = saved;
}

void deliver_video_interrupt(Cpu& c) {
    uint32_t handler = rd32(kInterruptTable + 4 * kVideoInterrupt);
    if (handler == 0 || !arm_video_interrupt()) {
        return;
    }
    Cpu saved = c;
    g_in_interrupt = true;
    c.msr &= ~kMsrExternalInterrupt;
    c.r[3] = kVideoInterrupt;
    c.r[4] = rd32(kCurrentContext);
    call(c, handler);
    c = saved;
    video::present();
    g_in_interrupt = false;
    c.r[3] = 0;
    call(c, symbol_address("OSSelectThread"));
    c = saved;
}

}

void poll_interrupts(Cpu& c) {
    g_poll_counter = 0;
    audio::update();
    if (g_in_interrupt || !(c.msr & kMsrExternalInterrupt)) {
        return;
    }
    if (ipc_pending()) {
        deliver_ipc_interrupt(c);
    }
    Clock::time_point now = Clock::now();
    if (now < g_next_retrace) {
        return;
    }
    std::chrono::microseconds period = retrace_period();
    g_next_retrace += period;
    if (g_next_retrace < now) {
        g_next_retrace = now + period;
    }
    deliver_video_interrupt(c);
}

}
