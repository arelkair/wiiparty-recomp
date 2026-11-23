#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "wp/audio.h"
#include "wp/cpu.h"
#include "wp/determinism.h"
#include "wp/dsp.h"
#include "wp/exi.h"
#include "wp/gx.h"
#include "wp/log.h"
#include "wp/hle.h"
#include "wp/ipc.h"
#include "wp/memory.h"
#include "wp/profile.h"
#include "wp/runtime.h"
#include "wp/si.h"
#include "wp/threads.h"
#include "wp/vclock.h"
#include "wp/video.h"

namespace wp {

uint32_t g_poll_counter = 1u << 10;

namespace {

constexpr uint32_t kMsrExternalInterrupt = 0x8000;
constexpr uint32_t kInterruptTable = 0x80003040;
constexpr uint32_t kVideoInterrupt = 24;
constexpr uint32_t kFinishInterrupt = 19;
constexpr uint32_t kSerialInterrupt = 20;
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
constexpr std::chrono::microseconds kBusyStep(5);

using Clock = std::chrono::steady_clock;
constexpr uint32_t kCurrentThread = 0x800000E4;
std::atomic<int64_t> g_idle_nanoseconds{0};

std::chrono::microseconds retrace_period() {
    return (rd16(kDisplayConfig) & kFormatMask) == kFormatPal ? kPalPeriod : kNtscPeriod;
}

Clock::time_point g_next_retrace = vclock::now() + kPalPeriod;
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
    enter_exception_context(context);
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
    forget_saved_context(context, &point);
    c = interrupted;
#ifdef WP_TRACE
    g_call_depth = trace_depth;
#endif
    g_in_interrupt = false;
}

void deliver_external_interrupt(Cpu& c, uint32_t index) {
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

double idle_share() {
    static Clock::time_point previous = Clock::now();
    Clock::time_point now = Clock::now();
    double elapsed = std::chrono::duration<double, std::nano>(now - previous).count();
    previous = now;
    double idle = static_cast<double>(g_idle_nanoseconds.exchange(0));
    return elapsed > 0.0 ? std::min(1.0, idle / elapsed) : -1.0;
}

void interrupt_left() {
    g_in_interrupt = false;
}

void poll_interrupts(Cpu& c) {
    static const uint32_t interval = [] {
        const char* setting = std::getenv("WP_POLL_INTERVAL");
        uint32_t value = setting ? static_cast<uint32_t>(std::strtoul(setting, nullptr, 10)) : 0;
        return value ? value : 1u << 10;
    }();
    g_poll_counter = interval;
    if (vclock::deterministic()) {
        if (rd32(kCurrentThread) == 0) {
            vclock::jump_to(g_next_retrace);
        } else {
            vclock::advance(kBusyStep);
        }
    }
    audio::update();
    dsp::update();
    log::watch_modules();
    ipc::update();
    if (g_in_interrupt || !(c.msr & kMsrExternalInterrupt)) {
        gx::process();
        return;
    }
    if (decrementer_due()) {
        deliver_decrementer(c);
    }
    if (ipc::interrupt_pending()) {
        deliver_external_interrupt(c, ipc::kInterrupt);
    }
    for (int delivered = 0; delivered < 3; delivered++) {
        uint32_t index = dsp::pending_interrupt();
        if (index == 0) {
            break;
        }
        deliver_external_interrupt(c, index);
    }
    if (gx::take_finish_interrupt()) {
        deliver_external_interrupt(c, kFinishInterrupt);
    }
    for (int delivered = 0; delivered < 3; delivered++) {
        uint32_t index = exi::pending_interrupt();
        if (index == 0) {
            break;
        }
        deliver_external_interrupt(c, index);
    }
    if (si::interrupt_pending()) {
        deliver_external_interrupt(c, kSerialInterrupt);
    }
    gx::process();
    Clock::time_point now = vclock::now();
    static Clock::time_point previous_poll = now;
    static bool previous_idle = false;
    bool idle = rd32(kCurrentThread) == 0;
    if (idle && previous_idle) {
        g_idle_nanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous_poll).count();
    }
    previous_poll = now;
    previous_idle = idle;
    if (now < g_next_retrace) {
        return;
    }
    std::chrono::microseconds period = retrace_period();
    g_next_retrace += period;
    if (g_next_retrace < now) {
        g_next_retrace = now + period;
    }
    si::poll();
    deliver_video_interrupt(c);
    if (vclock::deterministic()) {
        static uint64_t retraces = 0;
        static const Clock::time_point real_start = Clock::now();
        retraces++;
        determinism::retrace(retraces);
        if (!vclock::turbo()) {
            std::this_thread::sleep_until(real_start + period * static_cast<long>(retraces));
        }
    }
}

}
