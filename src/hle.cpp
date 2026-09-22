#include "wp/hle.h"

#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

#include "wp/format.h"
#include "wp/gx.h"
#include "wp/input.h"
#include "wp/ios.h"
#include "wp/threads.h"

namespace wp {

namespace {

constexpr uint32_t kRequestCallback = 0x20;
constexpr uint32_t kRequestArgument = 0x24;
constexpr uint32_t kIpcHeapSlot = 0xFFFF871C;
constexpr uint32_t kFreeRequestFunction = 0x80177300;
constexpr uint32_t kDrawDoneSlot = 0xFFFF8F38;
constexpr uint32_t kKpadStatusSize = 0xF0;
constexpr uint32_t kKpadTrigger = 0x04;
constexpr uint32_t kKpadRelease = 0x08;
constexpr uint32_t kKpadAcceleration = 0x0C;
constexpr uint32_t kKpadPointer = 0x20;
constexpr uint32_t kKpadHorizon = 0x34;
constexpr uint32_t kKpadDeviceType = 0x5C;
constexpr uint32_t kKpadError = 0x5D;
constexpr uint32_t kKpadPointerValid = 0x5E;
constexpr uint32_t kDeviceCore = 0;
constexpr uint32_t kDeviceNotFound = 0xFD;
constexpr uint32_t kChannelCount = 4;
constexpr const char* kSilentPrefix = "HleZero_";
constexpr float kPointerHeightScale = 1.0f;
constexpr uint32_t kWpadResetChannel = 0x8017afd0;
constexpr uint32_t kOsCreateAlarm = 0x8013f780;
constexpr uint32_t kWpadState = 0x802b62a0;
constexpr uint32_t kWpadBlockTable = kWpadState + 0x30;
constexpr uint32_t kWpadChannelFlags = kWpadState + 0x1040;
constexpr uint32_t kWpadChannelFlagCount = 16;
constexpr uint32_t kWpadBlocks = kWpadState + 0x1060;
constexpr uint32_t kWpadBlockSize = 0xbe0;
constexpr uint32_t kWpadBlockReset = 0x8e8;
constexpr uint32_t kWpadBlockAlarm = 0x928;
constexpr uint32_t kWpadBlockPending = 0xbae;
constexpr uint32_t kWpadInfoSize = 0x18;
constexpr uint32_t kWpadInfoBattery = 0x14;
constexpr uint32_t kWpadInfoLed = 0x15;
constexpr uint32_t kWpadFullBattery = 4;
constexpr int32_t kWpadNoController = -1;
uint32_t g_previous_buttons[kChannelCount] = {};

struct PendingRequest {
    uint32_t request;
    uint32_t result;
    uint32_t callback;
    uint32_t argument;
    std::chrono::steady_clock::time_point ready;
};

constexpr std::chrono::microseconds kIpcLatency(1000);

std::deque<PendingRequest> g_pending_ipc;

struct PendingWpadCallback {
    uint32_t callback;
    uint32_t channel;
    int32_t result;
    std::chrono::steady_clock::time_point ready;
};

constexpr std::chrono::milliseconds kWpadLatency(10);

std::deque<PendingWpadCallback> g_pending_wpad;

struct Replacement {
    const char* name;
    HleFunction function;
};

std::string read_guest_string(uint32_t address) {
    std::string text;
    for (char ch; (ch = static_cast<char>(rd8(address))) != 0 && text.size() < 256; address++) {
        text.push_back(ch);
    }
    return text;
}

void do_nothing(Cpu&) {}

void return_zero(Cpu& c) {
    c.r[3] = 0;
}

void return_one(Cpu& c) {
    c.r[3] = 1;
}

void ios_send(Cpu& c) {
    uint32_t request = c.r[3];
    bool asynchronous = c.r[4] != 0;
    int32_t result = ios::send(request);
    if (asynchronous && ios::never_completes(request)) {
        c.r[3] = 0;
        return;
    }
    if (!asynchronous) {
        c.r[3] = static_cast<uint32_t>(result);
        return;
    }
    g_pending_ipc.push_back({request, static_cast<uint32_t>(result), rd32(request + kRequestCallback), rd32(request + kRequestArgument),
                             std::chrono::steady_clock::now() + kIpcLatency});
    c.r[3] = 0;
}

void gx_draw_done(Cpu& c) {
    gx::process();
    wr8(c.r[13] + kDrawDoneSlot, 1);
}

void kpad_read(Cpu& c) {
    uint32_t channel = c.r[3];
    uint32_t buffer = c.r[4];
    uint32_t count = c.r[5];
    uint32_t error = c.r[6];
    uint32_t samples = 0;
    int32_t result = -1;
    if (buffer != 0 && count != 0) {
        std::memset(host(buffer), 0, kKpadStatusSize);
        if (channel < kChannelCount && input::connected(channel)) {
            input::Sample sample = input::sample(channel);
            uint32_t previous = g_previous_buttons[channel];
            g_previous_buttons[channel] = sample.buttons;
            wr32(buffer, sample.buttons);
            wr32(buffer + kKpadTrigger, sample.buttons & ~previous);
            wr32(buffer + kKpadRelease, previous & ~sample.buttons);
            wrf32(buffer + kKpadAcceleration + 8, 1.0f);
            wrf32(buffer + kKpadHorizon, 1.0f);
            wrf32(buffer + kKpadPointer, sample.pointer_x);
            wrf32(buffer + kKpadPointer + 4, sample.pointer_y * kPointerHeightScale);
            wr8(buffer + kKpadDeviceType, kDeviceCore);
            wr8(buffer + kKpadError, 0);
            wr8(buffer + kKpadPointerValid, sample.pointer_valid ? 1 : 0);
            samples = 1;
            result = 0;
        } else {
            wr8(buffer + kKpadDeviceType, kDeviceNotFound);
            wr8(buffer + kKpadError, 0xFF);
        }
    }
    if (error != 0) {
        wr32(error, static_cast<uint32_t>(result));
    }
    c.r[3] = samples;
}

void wpad_init(Cpu& c) {
    for (uint32_t i = 0; i < kWpadChannelFlagCount; i++) {
        wr8(kWpadChannelFlags + i, 0xFF);
    }
    for (uint32_t channel = 0; channel < kChannelCount; channel++) {
        uint32_t block = kWpadBlocks + channel * kWpadBlockSize;
        wr32(kWpadBlockTable + 4 * channel, block);
        wr32(block + kWpadBlockReset, 0);
        c.r[3] = channel;
        call(c, kWpadResetChannel);
        c.r[3] = block + kWpadBlockAlarm;
        call(c, kOsCreateAlarm);
        wr8(block + kWpadBlockPending, 0);
    }
}

void wpad_get_info_async(Cpu& c) {
    uint32_t channel = c.r[3];
    uint32_t info = c.r[4];
    uint32_t callback = c.r[5];
    bool present = channel < kChannelCount && input::connected(channel);
    if (!present) {
        if (callback != 0) {
            c.r[3] = channel;
            c.r[4] = static_cast<uint32_t>(kWpadNoController);
            call(c, callback);
        }
        c.r[3] = static_cast<uint32_t>(kWpadNoController);
        return;
    }
    if (info != 0) {
        std::memset(host(info), 0, kWpadInfoSize);
        wr8(info + kWpadInfoBattery, kWpadFullBattery);
        wr8(info + kWpadInfoLed, 1u << channel);
    }
    if (callback != 0) {
        g_pending_wpad.push_back({callback, channel, 0, std::chrono::steady_clock::now() + kWpadLatency});
    }
    c.r[3] = 0;
}

bool wpad_pending() {
    return !g_pending_wpad.empty() && g_pending_wpad.front().ready <= std::chrono::steady_clock::now();
}

bool request_pending() {
    return !g_pending_ipc.empty() && g_pending_ipc.front().ready <= std::chrono::steady_clock::now();
}

void wpad_probe(Cpu& c) {
    uint32_t channel = c.r[3];
    uint32_t type = c.r[4];
    bool present = channel < kChannelCount && input::connected(channel);
    if (type != 0) {
        wr32(type, present ? kDeviceCore : kDeviceNotFound);
    }
    c.r[3] = present ? 0 : static_cast<uint32_t>(-1);
}

void os_report(Cpu& c) {
    std::fputs(format_guest(c, c.r[3], 4).c_str(), stderr);
}

void os_panic(Cpu& c) {
    std::string message = "PANIC in " + read_guest_string(c.r[3]) + " on line " + std::to_string(c.r[4]) + ": " +
                          format_guest(c, c.r[5], 6);
    fatal_error(message.c_str());
}

void switch_fiber(Cpu& c) {
    uint32_t function = c.r[3];
    c.r[1] = c.r[4];
    call(c, function);
}

const Replacement kReplacements[] = {
    {"EXIInit", do_nothing},
    {"EXILock", return_zero},
    {"EXIUnlock", return_one},
    {"EXIProbe", return_zero},
    {"EXIGetID", return_zero},
    {"__OSInitAudioSystem", do_nothing},
    {"OSRealModeCall", do_nothing},
    {"IOSSendRequest", ios_send},
    {"OSReport", os_report},
    {"OSPanic", os_panic},
    {"WPADInit", wpad_init},
    {"KPADInit", wpad_init},
    {"OSLoadContext", load_context},
    {"OSSwitchFiber", switch_fiber},
    {"longjmp", long_jump},
    {"GXDrawDone", gx_draw_done},
    {"KPADReadEx", kpad_read},
    {"WPADProbe", wpad_probe},
    {"WPADGetInfoAsync", wpad_get_info_async},
};

}

bool ipc_pending() {
    return request_pending() || wpad_pending();
}

void ipc_deliver(Cpu& c) {
    while (wpad_pending()) {
        PendingWpadCallback pending = g_pending_wpad.front();
        g_pending_wpad.pop_front();
        c.r[3] = pending.channel;
        c.r[4] = static_cast<uint32_t>(pending.result);
        call(c, pending.callback);
    }
    while (request_pending()) {
        PendingRequest pending = g_pending_ipc.front();
        g_pending_ipc.pop_front();
        uint32_t heap = rd32(c.r[13] + kIpcHeapSlot);
        static const bool log_requests = std::getenv("WP_LOG_IOS") != nullptr;
        if (log_requests) {
            std::fprintf(stderr, "IOS complete request=%08x result=%d callback=%08x", pending.request, static_cast<int32_t>(pending.result), pending.callback);
            std::fputc(10, stderr);
        }
        if (pending.callback != 0) {
            c.r[3] = pending.result;
            c.r[4] = pending.argument;
            call(c, pending.callback);
            if (log_requests) {
                std::fprintf(stderr, "IOS callback returned request=%08x", pending.request);
                std::fputc(10, stderr);
            }
        }
        c.r[3] = heap;
        c.r[4] = pending.request;
        call(c, kFreeRequestFunction);
    }
}

HleFunction find_replacement(const char* name) {
    if (std::strncmp(name, kSilentPrefix, std::strlen(kSilentPrefix)) == 0) {
        return return_zero;
    }
    for (const Replacement& replacement : kReplacements) {
        if (std::strcmp(replacement.name, name) == 0) {
            return replacement.function;
        }
    }
    std::fprintf(stderr, "no runtime handler for replaced function %s\n", name);
    std::abort();
}

}
