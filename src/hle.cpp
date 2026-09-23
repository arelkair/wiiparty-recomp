#include "wp/hle.h"

#include <cstdio>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

#include "wp/format.h"
#include "wp/gx.h"
#include "wp/ios.h"
#include "wp/threads.h"

namespace wp {

namespace {

constexpr uint32_t kRequestCallback = 0x20;
constexpr uint32_t kRequestArgument = 0x24;
constexpr uint32_t kIpcHeapSlot = 0xFFFF871C;
constexpr uint32_t kFreeRequestFunction = 0x80177300;
constexpr uint32_t kDrawDoneSlot = 0xFFFF8F38;
constexpr const char* kSilentPrefix = "HleZero_";

struct PendingRequest {
    uint32_t request;
    uint32_t result;
    uint32_t callback;
    uint32_t argument;
    std::chrono::steady_clock::time_point ready;
};

constexpr std::chrono::microseconds kIpcLatency(1000);

std::deque<PendingRequest> g_pending_ipc;

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
    if (result == ios::kDeferred) {
        if (!asynchronous) {
            std::fprintf(stderr, "IOS synchronous request %08x cannot wait for a deferred reply\n", request);
        }
        c.r[3] = 0;
        return;
    }
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

bool request_pending() {
    return !g_pending_ipc.empty() && g_pending_ipc.front().ready <= std::chrono::steady_clock::now();
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
    {"OSLoadContext", load_context},
    {"OSSwitchFiber", switch_fiber},
    {"longjmp", long_jump},
    {"GXDrawDone", gx_draw_done},
};

}

bool ipc_pending() {
    ios::update();
    uint32_t request = 0;
    int32_t result = 0;
    while (ios::take_completion(request, result)) {
        g_pending_ipc.push_back({request, static_cast<uint32_t>(result), rd32(request + kRequestCallback), rd32(request + kRequestArgument),
                                 std::chrono::steady_clock::now()});
    }
    return request_pending();
}

void ipc_deliver(Cpu& c) {
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
