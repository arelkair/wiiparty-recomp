// Copyright 2010 Dolphin Emulator Project
// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wp/wiimote.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>

#include "wp/input.h"

namespace wp::wiimote {

namespace {

constexpr uint32_t kMaxWiimotes = 4;
constexpr uint32_t kEepromSize = 0x1700;
constexpr uint8_t kCameraAddress = 0x58;
constexpr uint8_t kSpeakerAddress = 0x51;
constexpr uint8_t kCameraDataOffset = 0x37;
constexpr uint8_t kCameraTrackingRegister = 0x30;
constexpr uint8_t kCameraModeRegister = 0x33;
constexpr uint8_t kCameraTrackingEnable = 0x08;
constexpr uint8_t kCameraModeBasic = 1;
constexpr uint8_t kCameraModeExtended = 3;
constexpr uint8_t kCameraModeFull = 5;
constexpr uint16_t kCameraWidth = 1024;
constexpr uint16_t kCameraHeight = 768;

constexpr uint8_t kReportRumble = 0x10;
constexpr uint8_t kReportLeds = 0x11;
constexpr uint8_t kReportMode = 0x12;
constexpr uint8_t kReportIrEnable = 0x13;
constexpr uint8_t kReportSpeakerEnable = 0x14;
constexpr uint8_t kReportRequestStatus = 0x15;
constexpr uint8_t kReportWriteData = 0x16;
constexpr uint8_t kReportReadData = 0x17;
constexpr uint8_t kReportSpeakerData = 0x18;
constexpr uint8_t kReportSpeakerMute = 0x19;
constexpr uint8_t kReportIrEnable2 = 0x1a;

constexpr uint8_t kInputStatus = 0x20;
constexpr uint8_t kInputReadReply = 0x21;
constexpr uint8_t kInputAck = 0x22;
constexpr uint8_t kInputCore = 0x30;

constexpr uint8_t kErrorSuccess = 0;
constexpr uint8_t kErrorBusy = 4;
constexpr uint8_t kErrorInvalidSpace = 6;
constexpr uint8_t kErrorNack = 7;
constexpr uint8_t kErrorInvalidAddress = 8;

constexpr uint16_t kIrLowX = 0x7F;
constexpr uint16_t kIrLowY = 0x5D;
constexpr uint16_t kIrHighX = 0x380;
constexpr uint16_t kIrHighY = 0x2A2;
constexpr uint8_t kAccelZeroG = 0x80;
constexpr uint8_t kAccelOneG = 0x9A;

constexpr std::array<uint8_t, 24> kEeprom16D0 = {0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00, 0x33, 0xCC, 0x44, 0xBB,
                                                 0x00, 0x00, 0x66, 0x99, 0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13};

constexpr auto kReportInterval = std::chrono::microseconds(5000);
constexpr double kReportSeconds = 0.005;
constexpr double kPi = 3.14159265358979323846;
constexpr double kGravity = 9.80665;
constexpr double kMaxTilt = kPi / 3.0;
constexpr double kTiltSpeed = 2.0 * kPi;
constexpr double kShakeFrequency = 6.0;
constexpr double kShakeTravel = 0.10;
constexpr double kSwingSeconds = 0.2;
constexpr double kSwingAcceleration = 2.5;
constexpr int kAccelMax = 1023;

struct ReadRequest {
    uint8_t space = 0;
    uint8_t slave = 0;
    uint16_t address = 0;
    uint16_t size = 0;
};

struct State {
    uint8_t mode = kInputCore;
    bool continuous = false;
    uint8_t leds = 0;
    bool ir = false;
    bool speaker = false;
    bool speaker_mute = false;
    uint16_t buttons = 0;
    uint16_t reported_buttons = 0;
    bool first_report = true;
    std::array<uint8_t, kEepromSize> eeprom{};
    std::array<uint8_t, 0x100> camera{};
    std::array<uint8_t, 0x100> speaker_registers{};
    ReadRequest read;
    std::chrono::steady_clock::time_point next_report{};
    double roll = 0.0;
    double pitch = 0.0;
    double shake_phase = 0.0;
    double swing_time = kSwingSeconds;
    double swing_direction = 0.0;
    uint32_t motion_held = 0;
    std::array<uint16_t, 3> accel{};
    std::array<uint16_t, 3> reported_accel{};
};

std::array<State, kMaxWiimotes> g_state;
bool g_log = std::getenv("WP_LOG_WIIMOTE") != nullptr;

template <size_t N>
void add_checksum(std::array<uint8_t, N>& data) {
    data[N - 1] = static_cast<uint8_t>(std::accumulate(data.begin(), data.end() - 1, 0x55));
}

void fill_eeprom(State& state) {
    state.eeprom.fill(0);
    std::array<uint8_t, 11> ir = {
        static_cast<uint8_t>(kIrLowX & 0xFF),
        static_cast<uint8_t>(kIrLowY & 0xFF),
        static_cast<uint8_t>(((kIrLowY & 0x300) >> 2) | ((kIrLowX & 0x300) >> 4) | ((kIrLowY & 0x300) >> 6) | ((kIrHighX & 0x300) >> 8)),
        static_cast<uint8_t>(kIrHighX & 0xFF),
        static_cast<uint8_t>(kIrLowY & 0xFF),
        static_cast<uint8_t>(kIrHighX & 0xFF),
        static_cast<uint8_t>(kIrHighY & 0xFF),
        static_cast<uint8_t>(((kIrHighY & 0x300) >> 2) | ((kIrHighX & 0x300) >> 4) | ((kIrHighY & 0x300) >> 6) | ((kIrLowX & 0x300) >> 8)),
        static_cast<uint8_t>(kIrLowX & 0xFF),
        static_cast<uint8_t>(kIrHighY & 0xFF),
        0,
    };
    add_checksum(ir);
    std::array<uint8_t, 10> accel = {kAccelZeroG, kAccelZeroG, kAccelZeroG, 0, kAccelOneG, kAccelOneG, kAccelOneG, 0, 0, 0};
    add_checksum(accel);
    std::copy(ir.begin(), ir.end(), state.eeprom.begin());
    std::copy(ir.begin(), ir.end(), state.eeprom.begin() + 11);
    std::copy(accel.begin(), accel.end(), state.eeprom.begin() + 22);
    std::copy(accel.begin(), accel.end(), state.eeprom.begin() + 32);
    std::copy(kEeprom16D0.begin(), kEeprom16D0.end(), state.eeprom.begin() + 0x16D0);
}

void put_buttons(std::vector<uint8_t>& report, uint16_t buttons) {
    report.push_back(static_cast<uint8_t>(buttons & 0xFF));
    report.push_back(static_cast<uint8_t>(buttons >> 8));
}

void send_input(const Sender& send, std::vector<uint8_t> payload) {
    std::vector<uint8_t> frame = {0xA1};
    frame.insert(frame.end(), payload.begin(), payload.end());
    send(frame);
}

void send_ack(State& state, const Sender& send, uint8_t report, uint8_t error) {
    std::vector<uint8_t> payload = {kInputAck};
    put_buttons(payload, state.buttons);
    payload.push_back(report);
    payload.push_back(error);
    send_input(send, payload);
}

void send_status(State& state, const Sender& send) {
    std::vector<uint8_t> payload = {kInputStatus};
    put_buttons(payload, state.buttons);
    uint8_t flags = static_cast<uint8_t>((state.speaker ? 0x04 : 0) | (state.ir ? 0x08 : 0) | (state.leds << 4));
    payload.push_back(flags);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(static_cast<uint8_t>(std::lround((1.0f + 0.013f) / 2.46f * 255.0f)));
    send_input(send, payload);
}

std::array<uint8_t, 0x100>* i2c_device(State& state, uint8_t slave) {
    if (slave == kCameraAddress) {
        return state.ir ? &state.camera : nullptr;
    }
    if (slave == kSpeakerAddress) {
        return &state.speaker_registers;
    }
    return nullptr;
}

bool process_read(State& state, const Sender& send) {
    uint16_t count = std::min<uint16_t>(16, state.read.size);
    if (count == 0) {
        return false;
    }
    std::vector<uint8_t> payload = {kInputReadReply};
    put_buttons(payload, state.buttons);
    uint8_t data[16] = {};
    uint8_t error = kErrorSuccess;
    if (state.read.space == 0) {
        if (state.read.address + state.read.size > kEepromSize) {
            error = kErrorInvalidAddress;
        } else {
            std::memcpy(data, state.eeprom.data() + state.read.address, count);
        }
    } else if (state.read.space == 1 || state.read.space == 2) {
        std::array<uint8_t, 0x100>* device = i2c_device(state, state.read.slave);
        uint8_t offset = static_cast<uint8_t>(state.read.address);
        if (device == nullptr || offset + count > 0x100) {
            error = kErrorNack;
        } else {
            std::memcpy(data, device->data() + offset, count);
        }
    } else {
        error = kErrorInvalidSpace;
    }
    uint8_t size_minus_one = error == kErrorSuccess ? static_cast<uint8_t>(count - 1) : 0x0F;
    payload.push_back(static_cast<uint8_t>((size_minus_one << 4) | error));
    payload.push_back(static_cast<uint8_t>(state.read.address >> 8));
    payload.push_back(static_cast<uint8_t>(state.read.address));
    payload.insert(payload.end(), data, data + 16);
    if (error != kErrorSuccess) {
        state.read.size = 0;
    } else {
        state.read.address = static_cast<uint16_t>(state.read.address + count);
        state.read.size = static_cast<uint16_t>(state.read.size - count);
    }
    send_input(send, payload);
    return true;
}

bool valid_mode(uint8_t mode) {
    return (mode >= 0x30 && mode <= 0x37) || mode == 0x3d;
}

struct Point {
    bool visible;
    uint16_t x;
    uint16_t y;
};

std::array<Point, 2> camera_points(const input::Sample& sample) {
    std::array<Point, 2> points{};
    if (!sample.pointer_valid) {
        return points;
    }
    constexpr float kHalfSeparation = 90.0f;
    constexpr float kSpan = 225.5f;
    constexpr float kSensorBarOffset = 102.0f;
    float mid_x = kCameraWidth / 2.0f - sample.pointer_x * kSpan;
    float mid_y = kCameraHeight / 2.0f + sample.pointer_y * kSpan + kSensorBarOffset;
    float xs[2] = {mid_x - kHalfSeparation, mid_x + kHalfSeparation};
    for (int i = 0; i < 2; i++) {
        long x = std::lround(xs[i]);
        long y = std::lround(mid_y);
        if (x >= 0 && y >= 0 && x < kCameraWidth && y < kCameraHeight) {
            points[i] = {true, static_cast<uint16_t>(x), static_cast<uint16_t>(y)};
        }
    }
    return points;
}

void update_camera(State& state, const input::Sample& sample) {
    uint8_t* data = state.camera.data() + kCameraDataOffset;
    std::fill(data, data + 36, 0xFF);
    if (state.camera[kCameraTrackingRegister] != kCameraTrackingEnable) {
        return;
    }
    std::array<Point, 2> points = camera_points(sample);
    uint8_t mode = state.camera[kCameraModeRegister];
    if (mode == kCameraModeBasic) {
        if (points[0].visible || points[1].visible) {
            uint16_t x1 = points[0].visible ? points[0].x : 0x3FF;
            uint16_t y1 = points[0].visible ? points[0].y : 0x3FF;
            uint16_t x2 = points[1].visible ? points[1].x : 0x3FF;
            uint16_t y2 = points[1].visible ? points[1].y : 0x3FF;
            data[0] = static_cast<uint8_t>(x1);
            data[1] = static_cast<uint8_t>(y1);
            data[2] = static_cast<uint8_t>(((y1 >> 8) << 6) | ((x1 >> 8) << 4) | ((y2 >> 8) << 2) | (x2 >> 8));
            data[3] = static_cast<uint8_t>(x2);
            data[4] = static_cast<uint8_t>(y2);
        }
    } else if (mode == kCameraModeExtended || mode == kCameraModeFull) {
        uint32_t stride = mode == kCameraModeExtended ? 3 : 9;
        for (uint32_t i = 0; i < 2; i++) {
            if (!points[i].visible) {
                continue;
            }
            uint8_t* object = data + i * stride;
            constexpr uint8_t kSize = 3;
            object[0] = static_cast<uint8_t>(points[i].x);
            object[1] = static_cast<uint8_t>(points[i].y);
            object[2] = static_cast<uint8_t>(((points[i].y >> 8) << 6) | ((points[i].x >> 8) << 4) | kSize);
            if (mode == kCameraModeFull) {
                object[3] = static_cast<uint8_t>(std::max(points[i].x - kSize, 0));
                object[4] = static_cast<uint8_t>(std::max(points[i].y - kSize, 0));
                object[5] = 0;
                object[6] = static_cast<uint8_t>(std::min(points[i].x + kSize, static_cast<int>(kCameraWidth)));
                object[7] = static_cast<uint8_t>(std::min(points[i].y + kSize, static_cast<int>(kCameraHeight)));
                object[8] = 0x20;
            }
        }
    }
}

bool accel_mode(uint8_t mode) {
    return mode == 0x31 || mode == 0x33 || mode == 0x35 || mode == 0x37;
}

double approach(double current, double target, double step) {
    if (current < target) {
        return std::min(current + step, target);
    }
    return std::max(current - step, target);
}

void update_motion(State& state, uint32_t held) {
    uint32_t pressed = held & ~state.motion_held;
    state.motion_held = held;
    double side = 0.0;
    double far = 0.0;
    if (held & input::kMotionTiltLeft) {
        side -= kMaxTilt;
    }
    if (held & input::kMotionTiltRight) {
        side += kMaxTilt;
    }
    if (held & input::kMotionTiltUp) {
        far += kMaxTilt;
    }
    if (held & input::kMotionTiltDown) {
        far -= kMaxTilt;
    }
    bool sideways = (held & input::kMotionSideways) != 0;
    double roll_target = sideways ? -far : side;
    double pitch_target = sideways ? side : far;
    state.roll = approach(state.roll, roll_target, kTiltSpeed * kReportSeconds);
    state.pitch = approach(state.pitch, pitch_target, kTiltSpeed * kReportSeconds);
    double accel[3] = {std::sin(state.roll) * std::cos(state.pitch), std::sin(state.pitch), std::cos(state.roll) * std::cos(state.pitch)};
    if (pressed & (input::kMotionSwingUp | input::kMotionSwingDown)) {
        state.swing_time = 0.0;
        state.swing_direction = (pressed & input::kMotionSwingUp) ? 1.0 : -1.0;
    }
    if (state.swing_time < kSwingSeconds) {
        double sign = state.swing_time < kSwingSeconds / 2.0 ? 1.0 : -1.0;
        accel[2] += sign * state.swing_direction * kSwingAcceleration;
        state.swing_time += kReportSeconds;
    }
    if (held & input::kMotionShake) {
        double omega = 2.0 * kPi * kShakeFrequency;
        double amplitude = omega * omega * (kShakeTravel / 2.0) / kGravity;
        double value = amplitude * std::sin(state.shake_phase);
        accel[0] += value;
        accel[1] += value;
        accel[2] += value;
        state.shake_phase += omega * kReportSeconds;
    } else {
        state.shake_phase = 0.0;
    }
    int zero = static_cast<int>(kAccelZeroG) << 2;
    int one = (static_cast<int>(kAccelOneG) << 2) - zero;
    for (int i = 0; i < 3; i++) {
        long value = std::lround(zero + accel[i] * one);
        state.accel[i] = static_cast<uint16_t>(std::clamp<long>(value, 0, kAccelMax));
    }
}

void send_data_report(State& state, const Sender& send, const input::Sample& sample) {
    uint8_t mode = state.mode;
    bool accel = mode == 0x31 || mode == 0x33 || mode == 0x35 || mode == 0x37;
    uint32_t ir_size = mode == 0x33 ? 12 : (mode == 0x36 || mode == 0x37) ? 10 : 0;
    uint32_t ext_size = 0;
    switch (mode) {
    case 0x32:
        ext_size = 8;
        break;
    case 0x34:
        ext_size = 19;
        break;
    case 0x35:
        ext_size = 16;
        break;
    case 0x36:
        ext_size = 9;
        break;
    case 0x37:
        ext_size = 6;
        break;
    case 0x3d:
        ext_size = 21;
        break;
    default:
        break;
    }
    std::vector<uint8_t> payload = {mode};
    if (mode != 0x3d) {
        uint16_t buttons = state.buttons;
        uint16_t x = state.accel[0];
        uint16_t y = state.accel[1];
        uint16_t z = state.accel[2];
        if (accel) {
            buttons = static_cast<uint16_t>(buttons | ((x & 3) << 5) | (((y >> 1) & 1) << 13) | (((z >> 1) & 1) << 14));
        }
        put_buttons(payload, buttons);
        if (accel) {
            payload.push_back(static_cast<uint8_t>(x >> 2));
            payload.push_back(static_cast<uint8_t>(y >> 2));
            payload.push_back(static_cast<uint8_t>(z >> 2));
        }
    }
    if (ir_size) {
        update_camera(state, sample);
        const uint8_t* data = state.camera.data() + kCameraDataOffset;
        if (state.ir) {
            payload.insert(payload.end(), data, data + ir_size);
        } else {
            payload.insert(payload.end(), ir_size, 0xFF);
        }
    }
    payload.insert(payload.end(), ext_size, 0xFF);
    send_input(send, payload);
}

}

void reset(uint32_t index) {
    if (index >= kMaxWiimotes) {
        return;
    }
    State& state = g_state[index];
    state = State{};
    fill_eeprom(state);
}

void output_report(uint32_t index, const uint8_t* data, uint32_t size, const Sender& send) {
    if (index >= kMaxWiimotes || size < 2) {
        return;
    }
    State& state = g_state[index];
    uint8_t id = data[0];
    const uint8_t* body = data + 1;
    uint32_t length = size - 1;
    if (g_log) {
        std::fprintf(stderr, "WIIMOTE %u output %02x size %u:", index, id, size);
        for (uint32_t i = 1; i < size && i < 12; i++) {
            std::fprintf(stderr, " %02x", data[i]);
        }
        std::fprintf(stderr, "\n");
    }
    bool ack = (body[0] & 0x02) != 0;
    switch (id) {
    case kReportRumble:
        break;
    case kReportLeds:
        state.leds = body[0] >> 4;
        if (ack) {
            send_ack(state, send, id, kErrorSuccess);
        }
        break;
    case kReportMode:
        if (length >= 2 && valid_mode(body[1])) {
            state.continuous = (body[0] & 0x04) != 0;
            state.mode = body[1];
            if (ack) {
                send_ack(state, send, id, kErrorSuccess);
            }
        }
        break;
    case kReportIrEnable:
        state.ir = (body[0] & 0x04) != 0;
        if (ack) {
            send_ack(state, send, id, kErrorSuccess);
        }
        break;
    case kReportIrEnable2:
        if (ack) {
            send_ack(state, send, id, kErrorSuccess);
        }
        break;
    case kReportSpeakerEnable:
        state.speaker = (body[0] & 0x04) != 0;
        if (ack) {
            send_ack(state, send, id, kErrorSuccess);
        }
        break;
    case kReportSpeakerMute:
        state.speaker_mute = (body[0] & 0x04) != 0;
        if (ack) {
            send_ack(state, send, id, kErrorSuccess);
        }
        break;
    case kReportRequestStatus:
        send_status(state, send);
        break;
    case kReportWriteData: {
        if (length < 21) {
            break;
        }
        uint8_t space = (body[0] >> 2) & 3;
        uint8_t slave = body[1] >> 1;
        uint16_t address = static_cast<uint16_t>((body[2] << 8) | body[3]);
        uint8_t count = body[4];
        if (count == 0 || count > 16) {
            break;
        }
        uint8_t error = kErrorSuccess;
        if (space == 0) {
            if (address + count > kEepromSize) {
                error = kErrorInvalidAddress;
            } else {
                std::memcpy(state.eeprom.data() + address, body + 5, count);
            }
        } else if (space == 1 || space == 2) {
            std::array<uint8_t, 0x100>* device = i2c_device(state, slave);
            uint8_t offset = static_cast<uint8_t>(address);
            if (device == nullptr || offset + count > 0x100) {
                error = kErrorNack;
            } else {
                std::memcpy(device->data() + offset, body + 5, count);
            }
        } else {
            error = kErrorInvalidSpace;
        }
        send_ack(state, send, id, error);
        break;
    }
    case kReportReadData: {
        if (length < 6) {
            break;
        }
        if (state.read.size) {
            send_ack(state, send, id, kErrorBusy);
            break;
        }
        state.read.space = (body[0] >> 2) & 3;
        state.read.slave = body[1] >> 1;
        state.read.address = static_cast<uint16_t>((body[2] << 8) | body[3]);
        state.read.size = static_cast<uint16_t>((body[4] << 8) | body[5]);
        process_read(state, send);
        break;
    }
    case kReportSpeakerData:
        break;
    default:
        if (g_log) {
            std::fprintf(stderr, "WIIMOTE %u unknown output report %02x\n", index, id);
        }
        break;
    }
}

void update(uint32_t index, const Sender& send) {
    if (index >= kMaxWiimotes) {
        return;
    }
    State& state = g_state[index];
    auto now = std::chrono::steady_clock::now();
    if (now < state.next_report) {
        return;
    }
    state.next_report = now + kReportInterval;
    input::Sample sample = input::sample(index);
    state.buttons = static_cast<uint16_t>(sample.buttons & 0x9F1F);
    update_motion(state, sample.buttons);
    if (process_read(state, send)) {
        return;
    }
    bool changed = state.buttons != state.reported_buttons || (accel_mode(state.mode) && state.accel != state.reported_accel) || state.first_report;
    if (state.mode == kInputCore && !state.continuous && !changed) {
        return;
    }
    state.reported_buttons = state.buttons;
    state.reported_accel = state.accel;
    state.first_report = false;
    send_data_report(state, send, sample);
}

}
