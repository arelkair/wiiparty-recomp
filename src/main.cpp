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

int main(int argc, char** argv) {
    std::string extracted = argc > 1 ? argv[1] : "extracted";
    int timeout_seconds = argc > 2 ? std::atoi(argv[2]) : 10;

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

    if (std::getenv("WP_PROFILE")) {
        wp::start_profiler();
    }
    std::thread([timeout_seconds] {
        std::this_thread::sleep_for(std::chrono::seconds(timeout_seconds));
        std::fprintf(stderr, "no progress after %d seconds\n", timeout_seconds);
        wp::print_call_stack();
        wp::print_profile();
        std::_Exit(3);
    }).detach();

    wp::Cpu cpu{};
    wp::init_threads(cpu);
    wp::call(cpu, entry);
    std::puts("entry returned");
    return 0;
}
