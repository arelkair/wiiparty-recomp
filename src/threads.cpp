#include "wp/threads.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "wp/function_table.h"

namespace wp {

namespace {

constexpr uint32_t kGprOffset = 0x000;
constexpr uint32_t kConditionOffset = 0x080;
constexpr uint32_t kLinkOffset = 0x084;
constexpr uint32_t kCounterOffset = 0x088;
constexpr uint32_t kExceptionOffset = 0x08C;
constexpr uint32_t kFprOffset = 0x090;
constexpr uint32_t kFpscrOffset = 0x194;
constexpr uint32_t kSrr0Offset = 0x198;
constexpr uint32_t kSrr1Offset = 0x19C;
constexpr uint32_t kGqrOffset = 0x1A4;
constexpr uint32_t kPairedOffset = 0x1C8;
constexpr size_t kFiberStackSize = 4u << 20;

struct SavedContext {
    void* fiber;
    std::jmp_buf* point;
};

std::map<uint32_t, SavedContext> g_saved;
Cpu* g_cpu = nullptr;
std::jmp_buf* g_resume_point = nullptr;
uint32_t g_resume_context = 0;

void store(const Cpu& c, uint32_t context) {
    for (uint32_t i = 0; i < 32; i++) {
        wr32(context + kGprOffset + 4 * i, c.r[i]);
        wr64(context + kFprOffset + 8 * i, fpr_bits(c.f[i]));
        wr64(context + kPairedOffset + 8 * i, fpr_bits(c.ps1[i]));
    }
    wr32(context + kConditionOffset, mfcr(c));
    wr32(context + kLinkOffset, c.lr);
    wr32(context + kCounterOffset, c.ctr);
    wr32(context + kExceptionOffset, mfxer(c));
    wr32(context + kFpscrOffset, c.fpscr);
    wr32(context + kSrr0Offset, c.lr);
    wr32(context + kSrr1Offset, c.msr);
    for (uint32_t i = 0; i < 8; i++) {
        wr32(context + kGqrOffset + 4 * i, c.spr[kSprGqr0 + i]);
    }
}

void restore(Cpu& c, uint32_t context) {
    for (uint32_t i = 0; i < 32; i++) {
        c.r[i] = rd32(context + kGprOffset + 4 * i);
        c.f[i] = fpr_from_bits(rd64(context + kFprOffset + 8 * i));
        c.ps1[i] = fpr_from_bits(rd64(context + kPairedOffset + 8 * i));
    }
    mtcrf(c, 0xFF, rd32(context + kConditionOffset));
    c.lr = rd32(context + kLinkOffset);
    c.ctr = rd32(context + kCounterOffset);
    mtxer(c, rd32(context + kExceptionOffset));
    c.fpscr = rd32(context + kFpscrOffset);
    for (uint32_t i = 0; i < 8; i++) {
        c.spr[kSprGqr0 + i] = rd32(context + kGqrOffset + 4 * i);
    }
}

[[noreturn]] void resume_pending() {
    std::jmp_buf* point = g_resume_point;
    g_resume_point = nullptr;
    std::longjmp(*point, 1);
}

VOID CALLBACK start_thread(PVOID parameter) {
    uint32_t context = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(parameter));
    Cpu& c = *g_cpu;
    restore(c, context);
    c.msr = rd32(context + kSrr1Offset);
    uint32_t entry = rd32(context + kSrr0Offset);
    uint32_t exit_function = c.lr;
    call(c, entry);
    call(c, exit_function);
    std::fputs("thread function returned without exiting\n", stderr);
    std::abort();
}

}

void init_threads(Cpu& c) {
    g_cpu = &c;
    ConvertThreadToFiber(nullptr);
}

void save_context(Cpu& c, std::jmp_buf* point) {
    uint32_t context = c.r[3];
    store(c, context);
    g_saved[context] = SavedContext{GetCurrentFiber(), point};
    c.r[3] = 0;
}

void resume_context(Cpu& c) {
    restore(c, g_resume_context);
    c.r[3] = 1;
}

void load_context(Cpu& c) {
    uint32_t context = c.r[3];
    auto it = g_saved.find(context);
    if (it == g_saved.end()) {
        void* fiber = CreateFiber(kFiberStackSize, start_thread, reinterpret_cast<void*>(static_cast<uintptr_t>(context)));
        SwitchToFiber(fiber);
        if (g_resume_point) {
            resume_pending();
        }
        return;
    }
    SavedContext saved = it->second;
    g_saved.erase(it);
    g_resume_point = saved.point;
    g_resume_context = context;
    if (saved.fiber != GetCurrentFiber()) {
        SwitchToFiber(saved.fiber);
    }
    if (g_resume_point) {
        resume_pending();
    }
}

uint32_t symbol_address(const char* name) {
    for (size_t i = 0; i < g_name_count; i++) {
        if (std::strcmp(g_name_table[i].name, name) == 0) {
            return g_name_table[i].address;
        }
    }
    std::fprintf(stderr, "unknown symbol %s\n", name);
    std::abort();
}

}
