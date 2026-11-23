#include "wp/frame_input.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#include "wp/log.h"
#include "wp/vclock.h"

namespace wp::frame_input {

namespace {

constexpr uint32_t kAllowedButtons = 0x00FF9F1F;
constexpr uint8_t kFlagPointer = 1;
constexpr uint8_t kFlagMotion = 2;
constexpr uint8_t kFlagPresent = 0x80;
constexpr float kPointerScale = 16384.0f;
constexpr float kAccelScale = 4096.0f;
constexpr size_t kRing = 64;
constexpr size_t kFlagsOffset = 14;
constexpr char kMagic[4] = {'W', 'P', 'I', 'N'};
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 8;

struct Slot {
    uint64_t frame = 0;
    bool valid = false;
    Packed data[kChannels];
};

struct State {
    std::mutex lock;
    Slot ring[kRing];
    Packed current[kChannels];
    RemoteSource remote[kChannels];
    bool started = false;
    std::FILE* record = nullptr;
    std::FILE* replay = nullptr;
    bool log_hashes = false;
};

State& state() {
    static State value;
    return value;
}

[[noreturn]] void stop(int code, const char* format, ...) {
    char message[256];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof message, format, arguments);
    va_end(arguments);
    log::write("replay", "%s", message);
    std::fprintf(stderr, "%s\n", message);
    std::fflush(stderr);
    std::_Exit(code);
}

int16_t to_fixed(float value, float scale) {
    float scaled = std::floor(value * scale + 0.5f);
    return static_cast<int16_t>(std::clamp(scaled, -32768.0f, 32767.0f));
}

void put16(uint8_t* out, int16_t value) {
    uint16_t bits = static_cast<uint16_t>(value);
    out[0] = static_cast<uint8_t>(bits);
    out[1] = static_cast<uint8_t>(bits >> 8);
}

int16_t get16(const uint8_t* in) {
    return static_cast<int16_t>(static_cast<uint16_t>(in[0] | (in[1] << 8)));
}

void put32(uint8_t* out, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out[i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

uint32_t get32(const uint8_t* in) {
    return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) | (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
}

Packed neutral(bool present) {
    Packed packed = pack(input::Sample{});
    if (!present) {
        packed.bytes[kFlagsOffset] = 0;
    }
    return packed;
}

uint32_t input_delay() {
    static const uint32_t value = [] {
        const char* text = std::getenv("WP_INPUT_DELAY");
        return text ? static_cast<uint32_t>(std::strtoul(text, nullptr, 10)) : 2u;
    }();
    return std::min<uint32_t>(value, kRing - 2);
}

uint8_t present_mask(const Packed* frame) {
    uint8_t mask = 0;
    for (size_t i = 0; i < kChannels; i++) {
        if (frame[i].bytes[kFlagsOffset] & kFlagPresent) {
            mask |= static_cast<uint8_t>(1u << i);
        }
    }
    return mask;
}

uint32_t frame_checksum(uint32_t frame, const Packed* data) {
    uint32_t hash = 0x811C9DC5u;
    auto mix = [&hash](uint8_t byte) {
        hash ^= byte;
        hash *= 0x01000193u;
    };
    for (int i = 0; i < 4; i++) {
        mix(static_cast<uint8_t>(frame >> (8 * i)));
    }
    uint8_t mask = present_mask(data);
    mix(mask);
    for (size_t i = 0; i < kChannels; i++) {
        if (mask & (1u << i)) {
            for (size_t b = 0; b < kPackedSize; b++) {
                mix(data[i].bytes[b]);
            }
        }
    }
    return hash;
}

void open_files(State& s) {
    s.log_hashes = std::getenv("WP_INPUT_HASH") != nullptr;
    if (const char* path = std::getenv("WP_INPUT_RECORD")) {
        s.record = std::fopen(path, "wb");
        if (!s.record) {
            stop(2, "replay: cannot create the record file %s", path);
        }
        uint8_t header[kHeaderSize];
        std::memcpy(header, kMagic, 4);
        header[4] = kVersion;
        header[5] = static_cast<uint8_t>(kChannels);
        header[6] = static_cast<uint8_t>(input_delay());
        header[7] = static_cast<uint8_t>(kPackedSize);
        std::fwrite(header, 1, kHeaderSize, s.record);
    }
    if (const char* path = std::getenv("WP_INPUT_REPLAY")) {
        s.replay = std::fopen(path, "rb");
        if (!s.replay) {
            stop(2, "replay: cannot open %s", path);
        }
        uint8_t header[kHeaderSize];
        if (std::fread(header, 1, kHeaderSize, s.replay) != kHeaderSize || std::memcmp(header, kMagic, 4) != 0) {
            stop(2, "replay: %s is not an input record (bad header)", path);
        }
        if (header[4] != kVersion) {
            stop(2, "replay: the record has format version %u, this build reads version %u", header[4], kVersion);
        }
        if (header[5] != kChannels || header[7] != kPackedSize) {
            stop(2, "replay: the record has %u channels of %u bytes, this build has %zu of %zu", header[5], header[7], kChannels, kPackedSize);
        }
        if (header[6] != input_delay()) {
            stop(2, "replay: the record was made with an input delay of %u, this run uses %u (set WP_INPUT_DELAY)", header[6], input_delay());
        }
    }
}

void read_replay_frame(State& s, uint64_t frame, Packed* out) {
    uint8_t head[5];
    size_t got = std::fread(head, 1, sizeof head, s.replay);
    if (got == 0) {
        stop(0, "replay: the record ends after frame %llu, the run reached frame %llu", static_cast<unsigned long long>(frame - 1), static_cast<unsigned long long>(frame));
    }
    if (got != sizeof head) {
        stop(2, "replay: the record is cut short in frame %llu (a frame header of %zu bytes)", static_cast<unsigned long long>(frame), got);
    }
    uint32_t stored_frame = get32(head);
    if (stored_frame != static_cast<uint32_t>(frame)) {
        stop(2, "replay: expected frame %llu, the file has %u", static_cast<unsigned long long>(frame), stored_frame);
    }
    uint8_t mask = head[4];
    for (size_t i = 0; i < kChannels; i++) {
        out[i] = neutral(false);
        if (mask & (1u << i)) {
            Packed packed;
            if (std::fread(packed.bytes, 1, kPackedSize, s.replay) != kPackedSize) {
                stop(2, "replay: the record is cut short in frame %llu (channel %zu)", static_cast<unsigned long long>(frame), i);
            }
            if (!(packed.bytes[kFlagsOffset] & kFlagPresent)) {
                stop(2, "replay: frame %llu channel %zu is marked present but its data is not", static_cast<unsigned long long>(frame), i);
            }
            out[i] = packed;
        }
    }
    uint8_t tail[4];
    if (std::fread(tail, 1, sizeof tail, s.replay) != sizeof tail) {
        stop(2, "replay: the record is cut short in frame %llu (checksum)", static_cast<unsigned long long>(frame));
    }
    uint32_t expected = frame_checksum(static_cast<uint32_t>(frame), out);
    uint32_t stored = get32(tail);
    if (expected != stored) {
        stop(2, "replay: frame %llu fails its checksum (stored %08x, computed %08x)", static_cast<unsigned long long>(frame), stored, expected);
    }
}

void write_record_frame(State& s, uint64_t frame, const Packed* in) {
    uint8_t head[5];
    put32(head, static_cast<uint32_t>(frame));
    uint8_t mask = present_mask(in);
    head[4] = mask;
    std::fwrite(head, 1, sizeof head, s.record);
    for (size_t i = 0; i < kChannels; i++) {
        if (mask & (1u << i)) {
            std::fwrite(in[i].bytes, 1, kPackedSize, s.record);
        }
    }
    uint8_t tail[4];
    put32(tail, frame_checksum(static_cast<uint32_t>(frame), in));
    std::fwrite(tail, 1, sizeof tail, s.record);
    std::fflush(s.record);
}

Slot& slot_for(State& s, uint64_t frame) {
    return s.ring[frame % kRing];
}

void store_local(State& s, uint64_t frame, uint32_t channel, const Packed& packed) {
    Slot& slot = slot_for(s, frame);
    if (!slot.valid || slot.frame != frame) {
        slot = Slot{};
        slot.frame = frame;
        slot.valid = true;
        for (size_t i = 0; i < kChannels; i++) {
            slot.data[i] = neutral(false);
        }
    }
    slot.data[channel] = packed;
}

}

bool latched() {
    return vclock::deterministic();
}

Packed pack(const input::Sample& sample) {
    Packed packed;
    uint32_t buttons = sample.buttons & kAllowedButtons;
    for (int i = 0; i < 4; i++) {
        packed.bytes[i] = static_cast<uint8_t>(buttons >> (8 * i));
    }
    put16(packed.bytes + 4, to_fixed(sample.pointer_valid ? sample.pointer_x : 0.0f, kPointerScale));
    put16(packed.bytes + 6, to_fixed(sample.pointer_valid ? sample.pointer_y : 0.0f, kPointerScale));
    for (int i = 0; i < 3; i++) {
        put16(packed.bytes + 8 + 2 * i, to_fixed(sample.accel[i], kAccelScale));
    }
    packed.bytes[kFlagsOffset] = static_cast<uint8_t>(kFlagPresent | (sample.pointer_valid ? kFlagPointer : 0) | (sample.motion_valid ? kFlagMotion : 0));
    return packed;
}

input::Sample unpack(const Packed& packed) {
    input::Sample sample;
    uint32_t buttons = 0;
    for (int i = 0; i < 4; i++) {
        buttons |= static_cast<uint32_t>(packed.bytes[i]) << (8 * i);
    }
    sample.buttons = buttons & kAllowedButtons;
    uint8_t flags = packed.bytes[kFlagsOffset];
    sample.pointer_valid = (flags & kFlagPointer) != 0;
    if (sample.pointer_valid) {
        sample.pointer_x = static_cast<float>(get16(packed.bytes + 4)) / kPointerScale;
        sample.pointer_y = static_cast<float>(get16(packed.bytes + 6)) / kPointerScale;
    }
    sample.motion_valid = (flags & kFlagMotion) != 0;
    for (int i = 0; i < 3; i++) {
        sample.accel[i] = static_cast<float>(get16(packed.bytes + 8 + 2 * i)) / kAccelScale;
    }
    return sample;
}

input::Sample quantize(const input::Sample& sample) {
    return unpack(pack(sample));
}

void set_remote_source(uint32_t channel, RemoteSource source) {
    if (channel >= kChannels) {
        return;
    }
    State& s = state();
    std::lock_guard<std::mutex> hold(s.lock);
    s.remote[channel] = std::move(source);
}

void begin_frame(uint64_t frame) {
    State& s = state();
    Packed resolved[kChannels];
    {
        std::lock_guard<std::mutex> hold(s.lock);
        if (!s.started) {
            s.started = true;
            open_files(s);
            for (uint32_t delay = 0; delay < input_delay(); delay++) {
                for (uint32_t channel = 0; channel < kChannels; channel++) {
                    store_local(s, frame + delay, channel, neutral(channel == 0));
                }
            }
        }
    }
    uint64_t target = frame + input_delay();
    Packed replayed[kChannels];
    bool replaying = false;
    {
        std::lock_guard<std::mutex> hold(s.lock);
        if (s.replay) {
            read_replay_frame(s, frame, replayed);
            replaying = true;
        }
    }
    for (uint32_t channel = 0; channel < kChannels; channel++) {
        RemoteSource remote;
        {
            std::lock_guard<std::mutex> hold(s.lock);
            remote = s.remote[channel];
        }
        if (replaying) {
            resolved[channel] = replayed[channel];
            continue;
        }
        if (remote) {
            Packed packed;
            while (!remote(frame, channel, packed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            resolved[channel] = packed;
            continue;
        }
        bool present = input::local_connected(channel);
        Packed captured = present ? pack(input::local_sample(channel)) : neutral(false);
        std::lock_guard<std::mutex> hold(s.lock);
        store_local(s, target, channel, captured);
        Slot& slot = slot_for(s, frame);
        resolved[channel] = slot.valid && slot.frame == frame ? slot.data[channel] : neutral(channel == 0);
    }
    std::lock_guard<std::mutex> hold(s.lock);
    for (size_t i = 0; i < kChannels; i++) {
        s.current[i] = resolved[i];
    }
    if (s.record) {
        write_record_frame(s, frame, resolved);
    }
    if (s.log_hashes) {
        log::write("input", "frame %llu hash %08x", static_cast<unsigned long long>(frame), frame_checksum(static_cast<uint32_t>(frame), resolved));
    }
}

bool connected(uint32_t channel) {
    if (channel >= kChannels) {
        return false;
    }
    State& s = state();
    std::lock_guard<std::mutex> hold(s.lock);
    if (!s.started) {
        return channel == 0;
    }
    return (s.current[channel].bytes[kFlagsOffset] & kFlagPresent) != 0;
}

input::Sample current(uint32_t channel) {
    if (channel >= kChannels) {
        return input::Sample{};
    }
    State& s = state();
    std::lock_guard<std::mutex> hold(s.lock);
    if (!s.started) {
        return input::Sample{};
    }
    return unpack(s.current[channel]);
}

}
