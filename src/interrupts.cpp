#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "wp/audio.h"
#include "wp/cpu.h"
#include "wp/dsp.h"
#include "wp/log.h"
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
constexpr uint32_t kExceptionTable = 0x80003000;
constexpr uint32_t kDecrementerException = 8;
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

void deliver_decrementer(Cpu& c) {
    decrementer_fired();
    static const bool log_interrupts = std::getenv("WP_LOG_IRQ") != nullptr;
    if (log_interrupts) {
        std::fprintf(stderr, "IRQ decrementer at tick %llu", static_cast<unsigned long long>(time_base()));
        std::fputc(10, stderr);
    }
    uint32_t handler = rd32(kExceptionTable + 4 * kDecrementerException);
    uint32_t context = rd32(kCurrentContext);
    if (handler == 0 || context == 0) {
        return;
    }
    Cpu interrupted = c;
#ifdef WP_TRACE
    size_t trace_depth = g_call_depth;
#endif
    std::jmp_buf point;
    c.r[3] = context;
    save_context(c, &point);
    if (setjmp(point) == 0) {
        c = interrupted;
        c.spr[26] = interrupted.lr;
        c.spr[27] = interrupted.msr;
        c.msr &= ~kMsrExternalInterrupt;
        c.r[3] = kDecrementerException;
        c.r[4] = context;
        g_in_interrupt = true;
        call(c, handler);
    }
    c = interrupted;
#ifdef WP_TRACE
    g_call_depth = trace_depth;
#endif
    g_in_interrupt = false;
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

void deliver_dsp_interrupt(Cpu& c, uint32_t index) {
    uint32_t handler = rd32(kInterruptTable + 4 * index);
    if (handler == 0) {
        return;
    }
    Cpu saved = c;
    g_in_interrupt = true;
    c.msr &= ~kMsrExternalInterrupt;
    c.r[3] = index;
    c.r[4] = rd32(kCurrentContext);
    call(c, handler);
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

void interrupt_left() {
    g_in_interrupt = false;
}

void poll_interrupts(Cpu& c) {
    g_poll_counter = 0;
    audio::update();
    dsp::update();
    log::watch_modules();
    if (g_in_interrupt || !(c.msr & kMsrExternalInterrupt)) {
        return;
    }
    if (decrementer_due()) {
        deliver_decrementer(c);
    }
    if (ipc_pending()) {
        deliver_ipc_interrupt(c);
    }
    for (int delivered = 0; delivered < 3; delivered++) {
        uint32_t index = dsp::pending_interrupt();
        if (index == 0) {
            break;
        }
        deliver_dsp_interrupt(c, index);
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
