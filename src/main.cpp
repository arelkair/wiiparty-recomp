#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "wp/boot.h"
#include "wp/cpu.h"
#include "wp/disc.h"
#include "wp/dol.h"
#include "wp/nand.h"
#include "wp/threads.h"
#include "wp/video.h"
#include "wp/watch.h"

namespace {

LONG WINAPI report_crash(EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    std::fprintf(stderr, "crash: exception %08lx at host address %p", record->ExceptionCode, record->ExceptionAddress);
    if (record->NumberParameters >= 2 && (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)) {
        std::fprintf(stderr, ", %s address %p", record->ExceptionInformation[0] ? "writing" : "reading",
                     reinterpret_cast<void*>(record->ExceptionInformation[1]));
    }
    std::fputc(10, stderr);
    wp::print_call_stack();
    wp::print_thread_stacks();
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

}

int main(int argc, char** argv) {
    SetUnhandledExceptionFilter(report_crash);
    std::string extracted = argc > 1 ? argv[1] : "extracted";
    int timeout_seconds = argc > 2 ? std::atoi(argv[2]) : 0;

    wp::g_memory = static_cast<uint8_t*>(std::calloc(wp::kMemorySize, 1));
    if (!wp::g_memory) {
        std::fputs("cannot allocate guest memory\n", stderr);
        return 1;
    }
    uint32_t entry = 0;
    if (!wp::load_dol(extracted + "/sys/main.dol", entry)) {
        std::fprintf(stderr, "cannot load %s/sys/main.dol\n", extracted.c_str());
        return 1;
    }
    if (!wp::init_boot_memory(extracted)) {
        std::fprintf(stderr, "cannot read boot files in %s/sys\n", extracted.c_str());
        return 1;
    }

    std::string nand_root = argc > 3 ? argv[3] : "game/nand";
    if (!wp::nand::mount(nand_root)) {
        std::fprintf(stderr, "cannot prepare the virtual NAND in %s\n", nand_root.c_str());
        return 1;
    }
    if (!wp::disc::mount(extracted)) {
        std::fprintf(stderr, "cannot read the disc layout in %s\n", extracted.c_str());
        return 1;
    }

    if (const char* watch = std::getenv("WP_WATCH")) {
        wp::start_watch(static_cast<uint32_t>(std::strtoul(watch, nullptr, 16)));
    }
    if (std::getenv("WP_PROFILE")) {
        wp::start_profiler();
    }
    if (timeout_seconds > 0) {
        std::thread([timeout_seconds] {
            std::this_thread::sleep_for(std::chrono::seconds(timeout_seconds));
            std::fprintf(stderr, "no progress after %d seconds\n", timeout_seconds);
            wp::print_call_stack();
            wp::print_thread_stacks();
            wp::print_profile();
            std::_Exit(3);
        }).detach();
    }

    wp::video::start();
    wp::Cpu cpu{};
    wp::init_threads(cpu);
    wp::call(cpu, entry);
    std::puts("entry returned");
    return 0;
}
