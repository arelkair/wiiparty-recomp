#include "wp/hle.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    if (!asynchronous) {
        c.r[3] = static_cast<uint32_t>(result);
        return;
    }
    uint32_t callback = rd32(request + kRequestCallback);
    uint32_t argument = rd32(request + kRequestArgument);
    uint32_t heap = rd32(c.r[13] + kIpcHeapSlot);
    if (callback != 0) {
        c.r[3] = static_cast<uint32_t>(result);
        c.r[4] = argument;
        call(c, callback);
    }
    c.r[3] = heap;
    c.r[4] = request;
    call(c, kFreeRequestFunction);
    c.r[3] = 0;
}

void gx_draw_done(Cpu& c) {
    gx::process();
    wr8(c.r[13] + kDrawDoneSlot, 1);
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
    {"WPADInit", do_nothing},
    {"KPADInit", do_nothing},
    {"OSLoadContext", load_context},
    {"OSSwitchFiber", switch_fiber},
    {"longjmp", long_jump},
    {"GXDrawDone", gx_draw_done},
};

}

HleFunction find_replacement(const char* name) {
    for (const Replacement& replacement : kReplacements) {
        if (std::strcmp(replacement.name, name) == 0) {
            return replacement.function;
        }
    }
    std::fprintf(stderr, "no runtime handler for replaced function %s\n", name);
    std::abort();
}

}
