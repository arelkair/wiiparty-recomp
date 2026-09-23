// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wp/dsp.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"
#include "wp/audio.h"
#include "wp/cpu.h"
#include "wp/log.h"
#include "wp/memory.h"

namespace wp::dsp {

namespace {

constexpr uint32_t kMailToDspHigh = 0x5000;
constexpr uint32_t kMailToDspLow = 0x5002;
constexpr uint32_t kMailFromDspHigh = 0x5004;
constexpr uint32_t kMailFromDspLow = 0x5006;
constexpr uint32_t kControl = 0x500A;
constexpr uint32_t kAramInfo = 0x5012;
constexpr uint32_t kAramMode = 0x5016;
constexpr uint32_t kAramRefresh = 0x501A;
constexpr uint32_t kAramDmaMainHigh = 0x5020;
constexpr uint32_t kAramDmaMainLow = 0x5022;
constexpr uint32_t kAramDmaAramHigh = 0x5024;
constexpr uint32_t kAramDmaAramLow = 0x5026;
constexpr uint32_t kAramDmaCountHigh = 0x5028;
constexpr uint32_t kAramDmaCountLow = 0x502A;
constexpr uint32_t kAudioDmaStartHigh = 0x5030;
constexpr uint32_t kAudioDmaStartLow = 0x5032;
constexpr uint32_t kAudioDmaControl = 0x5036;
constexpr uint32_t kAudioDmaBlocksLeft = 0x503A;

constexpr uint16_t kControlMask = 0x0C07;
constexpr uint16_t kControlReset = 0x0001;
constexpr uint16_t kControlHalt = 0x0004;
constexpr uint16_t kIntAudioDma = 0x0008;
constexpr uint16_t kIntAram = 0x0020;
constexpr uint16_t kIntDsp = 0x0080;
constexpr uint16_t kIntAll = kIntAudioDma | kIntAram | kIntDsp;
constexpr uint16_t kDmaState = 0x0200;

constexpr uint32_t kAudioControl = 0xCD006C00;
constexpr uint32_t kAudioDmaRate32k = 0x40;
constexpr uint32_t kMem1Mask = 0x01FFFFFF;
constexpr uint32_t kMem2Base = 0x10000000;
constexpr uint32_t kMem2Mask = 0x03FFFFFF;
constexpr double kDspClock = 729000000.0 / 6.0;
constexpr double kMaxLag = 0.02;
constexpr int kSlice = 512;
constexpr int kMailSlice = 512;
constexpr uint32_t kFramesPerBlock = 8;

using Clock = std::chrono::steady_clock;

struct AudioDma {
    uint32_t source = 0;
    uint16_t control = 0;
    uint32_t current = 0;
    uint16_t remaining = 0;
    bool source_written = false;
    bool started = false;
    Clock::time_point next_block{};
};

struct AramDma {
    uint32_t main = 0;
    uint32_t aram = 0;
    uint32_t count = 0;
};

struct Statistics {
    uint32_t buffers = 0;
    uint32_t late_starts = 0;
    double worst_start = 0.0;
    double dsp_seconds = 0.0;
    double dsp_cycles = 0.0;
    Clock::time_point since = Clock::now();
};

DSP::DSPCore* g_core = nullptr;
bool g_started = false;
uint16_t g_control = kControlHalt;
uint16_t g_aram_info = 0;
uint16_t g_aram_mode = 1;
uint16_t g_aram_refresh = 156;
AramDma g_aram_dma;
AudioDma g_audio_dma;
double g_owed = 0.0;
Clock::time_point g_last_update = Clock::now();
Statistics g_statistics;
bool g_log = std::getenv("WP_LOG_DSP") != nullptr;

bool load_rom(int id, uint16_t* words, size_t count) {
    HRSRC resource = FindResourceA(nullptr, MAKEINTRESOURCEA(id), MAKEINTRESOURCEA(10));
    if (!resource || SizeofResource(nullptr, resource) != count * 2) {
        return false;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(LockResource(LoadResource(nullptr, resource)));
    for (size_t i = 0; i < count; i++) {
        words[i] = static_cast<uint16_t>((bytes[2 * i] << 8) | bytes[2 * i + 1]);
    }
    return true;
}

void assert_interrupt_line() {
    g_poll_counter = kPollInterval;
}

void raise(uint16_t interrupt) {
    g_control = static_cast<uint16_t>(g_control | interrupt);
    assert_interrupt_line();
}

bool halted() {
    return !g_core || (g_core->DSPState().control_reg & kControlHalt);
}

void run_dsp(int cycles) {
    Clock::time_point start = Clock::now();
    g_core->RunCycles(cycles);
    g_statistics.dsp_seconds += std::chrono::duration<double>(Clock::now() - start).count();
    g_statistics.dsp_cycles += cycles;
}

void push_block(uint32_t source, bool enabled, uint32_t rate) {
    int16_t frames[kFramesPerBlock * 2];
    for (uint32_t i = 0; i < kFramesPerBlock; i++) {
        frames[2 * i] = enabled ? static_cast<int16_t>(rd16(source + 4 * i + 2)) : 0;
        frames[2 * i + 1] = enabled ? static_cast<int16_t>(rd16(source + 4 * i)) : 0;
    }
    audio::push(frames, kFramesPerBlock, rate);
}

void pump_audio_dma(Clock::time_point now) {
    uint32_t rate = (rd32(kAudioControl) & kAudioDmaRate32k) ? 32000 : 48000;
    const auto block = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(double(kFramesPerBlock) / rate));
    while (true) {
        bool enabled = (g_audio_dma.control & 0x8000) != 0;
        uint32_t source = 0;
        if (!enabled) {
            g_audio_dma.next_block = std::max(g_audio_dma.next_block, now - block);
            if (g_audio_dma.next_block + block > now) {
                return;
            }
            g_audio_dma.next_block += block;
        } else {
            if (g_audio_dma.remaining == 0) {
                if (!g_audio_dma.source_written) {
                    return;
                }
                g_audio_dma.current = g_audio_dma.source;
                g_audio_dma.remaining = g_audio_dma.control & 0x7FFF;
                g_audio_dma.source_written = false;
                g_audio_dma.started = false;
                raise(kIntAudioDma);
            }
            if (!g_audio_dma.started) {
                if (g_control & kIntAudioDma) {
                    return;
                }
                g_audio_dma.started = true;
                g_statistics.buffers++;
                double lateness = std::chrono::duration<double>(now - g_audio_dma.next_block).count();
                g_statistics.worst_start = std::max(g_statistics.worst_start, lateness);
                if (lateness > 0.001) {
                    g_statistics.late_starts++;
                }
            }
            if (g_audio_dma.next_block + block > now) {
                return;
            }
            g_audio_dma.next_block += block;
            source = g_audio_dma.current;
            g_audio_dma.remaining--;
            g_audio_dma.current += 32;
        }
        push_block(source, enabled, rate);
    }
}

void report(Clock::time_point now) {
    double seconds = std::chrono::duration<double>(now - g_statistics.since).count();
    if (seconds < 1.0) {
        return;
    }
    if (log::enabled()) {
        log::write("audio", "buffers %.1f/s, late starts %u, worst start %.2f ms, DSP %.1f Mcycles/s using %.1f%% of the CPU thread",
                   g_statistics.buffers / seconds, g_statistics.late_starts, g_statistics.worst_start * 1000.0,
                   g_statistics.dsp_cycles / seconds / 1e6, 100.0 * g_statistics.dsp_seconds / seconds);
    }
    g_statistics = Statistics{};
    g_statistics.since = now;
}

void start() {
    if (g_started) {
        return;
    }
    g_started = true;
    DSP::DSPInitOptions options;
    options.core_type = DSP::DSPInitOptions::CoreType::Interpreter;
    if (!load_rom(2, options.irom_contents.data(), options.irom_contents.size()) ||
        !load_rom(3, options.coef_contents.data(), options.coef_contents.size())) {
        std::fprintf(stderr, "DSP ROMs missing from the executable resources\n");
        return;
    }
    DSP::DSPCore* core = new DSP::DSPCore();
    if (!core->Initialize(options)) {
        std::fprintf(stderr, "DSP core failed to initialize\n");
        return;
    }
    core->Reset();
    DSP::InitInstructionTable();
    g_core = core;
    g_last_update = Clock::now();
    audio::start_output();
}

void do_aram_dma() {
    uint32_t main = g_aram_dma.main & 0x3FFFFFF;
    uint32_t aram = g_aram_dma.aram & 0x3FFFFFF;
    uint32_t count = g_aram_dma.count & 0x7FFFFFFF;
    bool to_main = (g_aram_dma.count & 0x80000000u) != 0;
    if (g_log) {
        std::fprintf(stderr, "DSP aram dma %s main %08x aram %08x count %x\n", to_main ? "in" : "out", main, aram, count);
    }
    for (uint32_t i = 0; i < count; i++) {
        uint8_t* aram_byte = host(kMem2Base | ((aram + i) & kMem2Mask));
        uint8_t* main_byte = host(main + i);
        if (to_main) {
            *main_byte = *aram_byte;
        } else {
            *aram_byte = *main_byte;
        }
    }
    g_aram_dma.main += count;
    g_aram_dma.aram += count;
    g_aram_dma.count &= 0x80000000u;
    g_control = static_cast<uint16_t>(g_control & ~kDmaState);
    raise(kIntAram);
}

void run_for_mail() {
    if (!halted()) {
        run_dsp(kMailSlice);
        g_owed -= kMailSlice;
    }
}

}

void update() {
    if (!g_core) {
        return;
    }
    Clock::time_point now = Clock::now();
    double seconds = std::chrono::duration<double>(now - g_last_update).count();
    g_last_update = now;
    g_owed = std::min(g_owed + seconds * kDspClock, kMaxLag * kDspClock);
    while (g_owed >= kSlice && !halted()) {
        run_dsp(kSlice);
        g_owed -= kSlice;
        DSP::SDSP& state = g_core->DSPState();
        if (state.GetAnalyzer().IsIdleSkip(state.pc)) {
            g_owed = 0.0;
        }
    }
    if (halted()) {
        g_owed = 0.0;
    }
    pump_audio_dma(now);
    report(now);
}

uint16_t read16(uint32_t address) {
    start();
    uint32_t reg = 0x5000 | (address & 0xFFE);
    switch (reg) {
    case kMailToDspHigh:
        if (g_core && (g_core->PeekMailbox(DSP::Mailbox::CPU) & 0x80000000u)) {
            run_for_mail();
        }
        return g_core ? g_core->ReadMailboxHigh(DSP::Mailbox::CPU) : 0;
    case kMailToDspLow:
        return g_core ? g_core->ReadMailboxLow(DSP::Mailbox::CPU) : 0;
    case kMailFromDspHigh:
        if (g_core && !(g_core->PeekMailbox(DSP::Mailbox::DSP) & 0x80000000u)) {
            run_for_mail();
        }
        return g_core ? g_core->ReadMailboxHigh(DSP::Mailbox::DSP) : 0;
    case kMailFromDspLow:
        return g_core ? g_core->ReadMailboxLow(DSP::Mailbox::DSP) : 0;
    case kControl: {
        uint16_t engine = g_core ? g_core->GetInterpreter().ReadControlRegister() : kControlHalt;
        return static_cast<uint16_t>((g_control & ~kControlMask) | (engine & kControlMask));
    }
    case kAramInfo:
        return g_aram_info;
    case kAramMode:
        return g_aram_mode;
    case kAramRefresh:
        return g_aram_refresh;
    case kAramDmaMainHigh:
        return static_cast<uint16_t>(g_aram_dma.main >> 16);
    case kAramDmaMainLow:
        return static_cast<uint16_t>(g_aram_dma.main);
    case kAramDmaAramHigh:
        return static_cast<uint16_t>(g_aram_dma.aram >> 16);
    case kAramDmaAramLow:
        return static_cast<uint16_t>(g_aram_dma.aram);
    case kAramDmaCountHigh:
        return static_cast<uint16_t>(g_aram_dma.count >> 16);
    case kAramDmaCountLow:
        return static_cast<uint16_t>(g_aram_dma.count);
    case kAudioDmaStartHigh:
        return static_cast<uint16_t>(g_audio_dma.source >> 16);
    case kAudioDmaStartLow:
        return static_cast<uint16_t>(g_audio_dma.source);
    case kAudioDmaControl:
        return g_audio_dma.control;
    case kAudioDmaBlocksLeft:
        return static_cast<uint16_t>(g_audio_dma.remaining > 0 ? g_audio_dma.remaining - 1 : 0);
    default:
        return 0;
    }
}

void write16(uint32_t address, uint16_t value) {
    start();
    uint32_t reg = 0x5000 | (address & 0xFFE);
    switch (reg) {
    case kMailToDspHigh:
        if (g_core) {
            g_core->WriteMailboxHigh(DSP::Mailbox::CPU, value);
        }
        break;
    case kMailToDspLow:
        if (g_core) {
            g_core->WriteMailboxLow(DSP::Mailbox::CPU, value);
        }
        break;
    case kControl: {
        uint16_t engine = kControlHalt;
        if (g_core) {
            DSP::Interpreter::Interpreter& interpreter = g_core->GetInterpreter();
            interpreter.WriteControlRegister(value);
            if (value & DSP::CR_EXTERNAL_INT) {
                g_core->CheckExternalInterrupt();
                g_core->CheckExceptions();
            }
            engine = interpreter.ReadControlRegister();
        }
        if (value & kControlReset) {
            g_audio_dma.control = 0;
        }
        uint16_t kept = static_cast<uint16_t>(g_control & (kIntAll | kDmaState));
        uint16_t acknowledged = static_cast<uint16_t>(value & kIntAll);
        g_control = static_cast<uint16_t>((value & ~(kIntAll | kDmaState | kControlMask)) | (engine & kControlMask) | (kept & ~acknowledged));
        break;
    }
    case kAramInfo:
        g_aram_info = value & 0x007F;
        break;
    case kAramRefresh:
        g_aram_refresh = value & 0x07FF;
        break;
    case kAramDmaMainHigh:
        g_aram_dma.main = (g_aram_dma.main & 0xFFFF) | (static_cast<uint32_t>(value & 0x03FF) << 16);
        break;
    case kAramDmaMainLow:
        g_aram_dma.main = (g_aram_dma.main & 0xFFFF0000u) | (value & 0xFFE0);
        break;
    case kAramDmaAramHigh:
        g_aram_dma.aram = (g_aram_dma.aram & 0xFFFF) | (static_cast<uint32_t>(value & 0x03FF) << 16);
        break;
    case kAramDmaAramLow:
        g_aram_dma.aram = (g_aram_dma.aram & 0xFFFF0000u) | (value & 0xFFE0);
        break;
    case kAramDmaCountHigh:
        g_aram_dma.count = (g_aram_dma.count & 0xFFFF) | (static_cast<uint32_t>(value & 0x83FF) << 16);
        break;
    case kAramDmaCountLow:
        g_aram_dma.count = (g_aram_dma.count & 0xFFFF0000u) | (value & 0xFFE0);
        do_aram_dma();
        break;
    case kAudioDmaStartHigh:
        g_audio_dma.source = (g_audio_dma.source & 0xFFFF) | (static_cast<uint32_t>(value & 0x1FFF) << 16);
        break;
    case kAudioDmaStartLow:
        g_audio_dma.source = (g_audio_dma.source & 0xFFFF0000u) | (value & 0xFFE0);
        g_audio_dma.source_written = true;
        break;
    case kAudioDmaControl: {
        bool already = (g_audio_dma.control & 0x8000) != 0;
        g_audio_dma.control = value;
        if (!already && (value & 0x8000)) {
            g_audio_dma.current = g_audio_dma.source;
            g_audio_dma.remaining = value & 0x7FFF;
            g_audio_dma.source_written = false;
            g_audio_dma.started = false;
            g_audio_dma.next_block = Clock::now();
            raise(kIntAudioDma);
        }
        break;
    }
    default:
        break;
    }
}

uint32_t pending_interrupt() {
    uint16_t active = static_cast<uint16_t>((g_control >> 1) & g_control & kIntAll);
    if (active & kIntDsp) {
        return kInterruptDsp;
    }
    if (active & kIntAram) {
        return kInterruptAram;
    }
    if (active & kIntAudioDma) {
        return kInterruptAudioDma;
    }
    return 0;
}

}

namespace DSP::Host {

u8 ReadHostMemory(u32 address) {
    if (address & wp::dsp::kMem2Base) {
        return wp::rd8(wp::dsp::kMem2Base | (address & wp::dsp::kMem2Mask));
    }
    return wp::rd8(address & wp::dsp::kMem1Mask);
}

void WriteHostMemory(u8 value, u32 address) {
    wp::wr8(wp::dsp::kMem2Base | (address & wp::dsp::kMem2Mask), value);
}

void DMAToDSP(u16* destination, u32 address, u32 size) {
    for (u32 i = 0; i < size / 2; i++) {
        destination[i] = wp::rd16(address + 2 * i);
    }
}

void DMAFromDSP(const u16* source, u32 address, u32 size) {
    for (u32 i = 0; i < size / 2; i++) {
        wp::wr16(address + 2 * i, source[i]);
    }
}

void OSD_AddMessage(std::string message, u32) {
    std::fprintf(stderr, "DSP %s\n", message.c_str());
}

bool OnThread() {
    return false;
}

bool IsWiiHost() {
    return true;
}

void InterruptRequest() {
    wp::dsp::raise(wp::dsp::kIntDsp);
}

u64 TimeBase() {
    return wp::time_base();
}

void CodeLoaded(DSPCore& dsp, u32 address, size_t size) {
    CodeLoaded(dsp, wp::host(address), size);
}

void CodeLoaded(DSPCore& dsp, const u8* pointer, size_t size) {
    SDSP& state = dsp.DSPState();
    uint32_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        crc = (crc * 31) ^ pointer[i];
    }
    state.SetIRAMCRC(crc);
    if (wp::dsp::g_log) {
        std::fprintf(stderr, "DSP code loaded, %zu bytes, checksum %08x\n", size, crc);
    }
    wp::log::write("audio", "DSP microcode loaded, %u bytes, checksum %08x", static_cast<unsigned>(size), crc);
    dsp.ClearIRAM();
    state.GetAnalyzer().Analyze(state);
}

}
