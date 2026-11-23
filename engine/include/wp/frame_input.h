#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "wp/input.h"

namespace wp::frame_input {

constexpr size_t kChannels = 4;
constexpr size_t kPackedSize = 15;

struct Packed {
    uint8_t bytes[kPackedSize] = {};
};

using RemoteSource = std::function<bool(uint64_t frame, uint32_t channel, Packed& out)>;

bool latched();
void begin_frame(uint64_t frame);
bool connected(uint32_t channel);
input::Sample current(uint32_t channel);

Packed pack(const input::Sample& sample);
input::Sample unpack(const Packed& packed);
input::Sample quantize(const input::Sample& sample);

void set_remote_source(uint32_t channel, RemoteSource source);

}
