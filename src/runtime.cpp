#include "wp/cpu.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <thread>
#include <vector>

#include "wp/function_table.h"
#include "wp/modules.h"

namespace wp {

uint8_t* g_memory = nullptr;

namespace {

constexpr uint64_t kTimeBaseHz = 60750000;
constexpr size_t kReportedFrames = 24;

void dump_memory() {
    const char* path = std::getenv("WP_DUMP");
    if (!path) {
        return;
    }
    std::FILE* file = std::fopen(path, "wb");
    if (file) {
        std::fwrite(g_memory, 1, kMemorySize, file);
        std::fclose(file);
    }
}

[[noreturn]] void fail(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
    print_call_stack();
    dump_memory();
    std::abort();
}

}

const char* find_name(uint32_t address) {
    const NameEntry* end = g_name_table + g_name_count;
    const NameEntry* it = std::lower_bound(
        g_name_table, end, address,
        [](const NameEntry& entry, uint32_t value) { return entry.address < value; });
    return it != end && it->address == address ? it->name : "";
}

#ifdef WP_TRACE
uint32_t g_call_stack[kCallStackSize];
volatile size_t g_call_depth = 0;

void print_call_stack() {
    size_t depth = g_call_depth;
    std::fprintf(stderr, "call stack depth %zu (innermost first):", depth);
    for (size_t i = 0; i < kReportedFrames && i < depth; i++) {
        size_t index = depth - 1 - i;
        if (index < kCallStackSize) {
            uint32_t address = g_call_stack[index];
            const char* name = find_name(address);
            std::fprintf(stderr, " %08x%s%s%s", address, *name ? "(" : "", name, *name ? ")" : "");
        }
    }
    std::fputc('\n', stderr);
}

namespace {
std::map<uint32_t, uint32_t> g_profile;
std::atomic<bool> g_sampling{false};
}

void start_profiler() {
    g_sampling = true;
    std::thread([] {
        while (g_sampling) {
            size_t depth = g_call_depth;
            if (depth > 0 && depth <= kCallStackSize) {
                g_profile[g_call_stack[depth - 1]]++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }).detach();
}

void print_profile() {
    g_sampling = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    dump_memory();
    std::vector<std::pair<uint32_t, uint32_t>> rows(g_profile.begin(), g_profile.end());
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::fprintf(stderr, "profile (samples per innermost function):\n");
    for (size_t i = 0; i < rows.size() && i < 15; i++) {
        const char* name = find_name(rows[i].first);
        std::fprintf(stderr, "  %08x %6u %s\n", rows[i].first, rows[i].second, name);
    }
}
#else
void print_call_stack() {}
void start_profiler() {}
void print_profile() {}
#endif

void call(Cpu& c, uint32_t address) {
    const FunctionEntry* end = g_function_table + g_function_count;
    const FunctionEntry* it = std::lower_bound(
        g_function_table, end, address,
        [](const FunctionEntry& entry, uint32_t value) { return entry.address < value; });
    if (it == end || it->address != address) {
        if (!call_module_function(c, address)) {
            missing_function(c, address);
        }
        return;
    }
    it->function(c);
}

void missing_function(Cpu&, uint32_t address) {
    describe_loaded_modules(address);
    fail("call to unknown function %08x", address);
}

void illegal_instruction(Cpu&, uint32_t address, uint32_t word) {
    fail("illegal instruction at %08x: %08x", address, word);
}

void unsupported_instruction(Cpu&, uint32_t address, const char* name) {
    fail("unsupported instruction at %08x: %s", address, name);
}

void unresolved_jump(Cpu&, uint32_t target) {
    fail("unresolved jump to %08x", target);
}

void fatal_error(const char* message) {
    fail("%s", message);
}

void trap(Cpu&, uint32_t address) {
    fail("trap at %08x", address);
}

void system_call(Cpu&) {}

uint64_t time_base() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point origin = clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - origin).count();
    return static_cast<uint64_t>(elapsed) * kTimeBaseHz / 1000000000ull;
}

}
