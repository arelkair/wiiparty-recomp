// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wp/hollywood.h"

namespace wp::hollywood {

namespace {

constexpr uint32_t kGpioOut = 0xCD8000C0;
constexpr uint32_t kGpioDirection = 0xCD8000C4;
constexpr uint32_t kGpioIn = 0xCD8000C8;
constexpr uint32_t kUnknown180 = 0xCD800180;
constexpr uint32_t kUnknown1CC = 0xCD8001CC;
constexpr uint32_t kUnknown1D0 = 0xCD8001D0;

constexpr uint32_t kPower = 0x1;
constexpr uint32_t kShutdown = 0x2;
constexpr uint32_t kFan = 0x4;
constexpr uint32_t kDcDc = 0x8;
constexpr uint32_t kDiSpin = 0x10;
constexpr uint32_t kSlotLed = 0x20;
constexpr uint32_t kSlotIn = 0x80;
constexpr uint32_t kSensorBar = 0x100;
constexpr uint32_t kDoEject = 0x200;
constexpr uint32_t kEepromSelect = 0x400;
constexpr uint32_t kEepromClock = 0x800;
constexpr uint32_t kEepromOut = 0x1000;
constexpr uint32_t kVideoEncoderClock = 0x4000;
constexpr uint32_t kVideoEncoderData = 0x8000;
constexpr uint32_t kDebug = 0xFF0000;

constexpr uint32_t kBroadwayOwned = kSlotLed | kSlotIn | kSensorBar | kDoEject | kVideoEncoderClock | kVideoEncoderData;
constexpr uint32_t kOutputPins = kPower | kShutdown | kFan | kDcDc | kDiSpin | kSlotLed | kSensorBar | kDoEject | kEepromSelect | kEepromClock |
                                 kEepromOut | kVideoEncoderClock | kVideoEncoderData | kDebug;

uint32_t g_out = 0;
uint32_t g_direction = kOutputPins;

}

bool owns(uint32_t address) {
    return address == kGpioOut || address == kGpioDirection || address == kGpioIn || address == kUnknown180 || address == kUnknown1CC ||
           address == kUnknown1D0;
}

uint32_t read32(uint32_t address) {
    switch (address) {
    case kGpioOut:
        return g_out;
    case kGpioDirection:
        return g_direction;
    case kGpioIn:
        return kSlotIn;
    default:
        return 0;
    }
}

void write32(uint32_t address, uint32_t value) {
    if (address == kGpioOut) {
        g_out = (value & kBroadwayOwned) | (g_out & ~kBroadwayOwned);
    } else if (address == kGpioDirection) {
        g_direction = (value & kBroadwayOwned) | (g_direction & ~kBroadwayOwned);
    }
}

}
