#include "wp/hle.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "wp/format.h"
#include "wp/threads.h"

namespace wp {

namespace {

constexpr const char* kSilentPrefix = "HleZero_";

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
    {"OSRealModeCall", do_nothing},
    {"OSReport", os_report},
    {"OSPanic", os_panic},
    {"OSLoadContext", load_context},
    {"OSSwitchFiber", switch_fiber},
    {"longjmp", long_jump},
};

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
