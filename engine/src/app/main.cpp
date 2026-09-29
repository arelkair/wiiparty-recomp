#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "wp/big_stack.h"
#include "wp/boot.h"
#include "wp/cpu.h"
#include "wp/disc.h"
#include "wp/dol.h"
#include "wp/game.h"
#include "wp/nand.h"
#include "wp/ipc.h"
#include "wp/settings.h"
#include "wp/threads.h"
#include "wp/video.h"
#include "wp/watch.h"

namespace {

#ifdef _WIN32
LONG WINAPI report_crash(EXCEPTION_POINTERS* info) {
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    std::fprintf(stderr, "crash: exception %08lx at host address %p (module offset %llx)", record->ExceptionCode, record->ExceptionAddress,
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(record->ExceptionAddress) - reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr))));
    if (record->NumberParameters >= 2 && (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)) {
        std::fprintf(stderr, ", %s address %p", record->ExceptionInformation[0] ? "writing" : "reading",
                     reinterpret_cast<void*>(record->ExceptionInformation[1]));
    }
    std::fputc(10, stderr);
    wp::print_guest_registers();
    wp::print_call_stack();
    wp::print_thread_stacks();
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#else
void report_crash(int signal) {
    std::fprintf(stderr, "crash: signal %d", signal);
    std::fputc(10, stderr);
    wp::print_guest_registers();
    wp::print_call_stack();
    wp::print_thread_stacks();
    std::fflush(stderr);
    std::_Exit(128 + signal);
}
#endif

}

int run(int argc, char** argv) {
    wp::settings::load(wp::game::description().settings_file);
    std::string extracted = argc > 1 ? argv[1] : wp::game::description().data_directory;
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

    std::string nand_root = argc > 3 ? argv[3] : wp::game::description().nand_directory;
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
            wp::ipc::report();
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

int main(int argc, char** argv) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(report_crash);
    return run(argc, argv);
#else
    std::signal(SIGSEGV, report_crash);
    std::signal(SIGBUS, report_crash);
    std::signal(SIGILL, report_crash);
    std::signal(SIGFPE, report_crash);
    int result = 0;
    wp::run_with_stack(size_t{512} << 20, [&] { result = run(argc, argv); });
    return result;
#endif
}
