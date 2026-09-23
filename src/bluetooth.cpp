// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wp/bluetooth.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>

#include "wp/ios.h"
#include "wp/memory.h"

namespace wp::bluetooth {

namespace {

constexpr const char* kDevicePath = "/dev/usb/oh1/57e/305";
constexpr uint32_t kVectorSize = 8;

constexpr uint32_t kControlMessage = 0;
constexpr uint32_t kBulkMessage = 1;
constexpr uint32_t kInterruptMessage = 2;

constexpr uint8_t kEndpointEvents = 0x81;
constexpr uint8_t kEndpointAclIn = 0x82;
constexpr uint8_t kEndpointAclOut = 0x02;

constexpr uint8_t kEventCommandComplete = 0x0E;
constexpr uint8_t kEventCommandStatus = 0x0F;
constexpr uint8_t kEventReturnLinkKeys = 0x15;

constexpr uint16_t kCommandReset = 0x0C03;
constexpr uint16_t kCommandReadBufferSize = 0x1005;
constexpr uint16_t kCommandReadLocalVersion = 0x1001;
constexpr uint16_t kCommandReadBdAddr = 0x1009;
constexpr uint16_t kCommandReadLocalFeatures = 0x1003;
constexpr uint16_t kCommandReadStoredLinkKey = 0x0C0D;
constexpr uint16_t kCommandDeleteStoredLinkKey = 0x0C12;
constexpr uint16_t kCommandWriteScanEnable = 0x0C1A;
constexpr uint16_t kCommandWriteLinkSupervisionTimeout = 0x0C37;
constexpr uint16_t kCommandInquiry = 0x0401;

constexpr uint16_t kAclPacketSize = 339;
constexpr uint16_t kAclPacketCount = 10;
constexpr uint8_t kScoPacketSize = 64;
constexpr uint16_t kScoPacketCount = 0;

constexpr uint8_t kControllerAddress[6] = {0xff, 0x00, 0x79, 0x19, 0x02, 0x11};

struct Endpoint {
    uint32_t request = 0;
    uint32_t data = 0;
    uint32_t length = 0;
};

struct Completion {
    uint32_t request;
    int32_t result;
};

Endpoint g_events;
Endpoint g_acl_in;
std::deque<std::vector<uint8_t>> g_event_queue;
std::deque<Completion> g_completions;
uint8_t g_scan_enable = 0;
bool g_log = std::getenv("WP_LOG_BT") != nullptr;

uint32_t in_vector(uint32_t vectors, uint32_t index) {
    return rd32(vectors + index * kVectorSize);
}

uint32_t io_vector(uint32_t vectors, uint32_t input_count, uint32_t index) {
    return rd32(vectors + (input_count + index) * kVectorSize);
}

uint32_t io_vector_size(uint32_t vectors, uint32_t input_count, uint32_t index) {
    return rd32(vectors + (input_count + index) * kVectorSize + 4);
}

void put16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void deliver_events() {
    while (g_events.request != 0 && !g_event_queue.empty()) {
        const std::vector<uint8_t>& event = g_event_queue.front();
        uint32_t size = static_cast<uint32_t>(event.size());
        if (size > g_events.length) {
            size = g_events.length;
        }
        std::memcpy(host(g_events.data), event.data(), size);
        g_completions.push_back({g_events.request, static_cast<int32_t>(size)});
        g_events = Endpoint{};
        g_event_queue.pop_front();
    }
}

void queue_event(std::vector<uint8_t> event) {
    if (g_log) {
        std::fprintf(stderr, "BT event %02x length %u\n", event[0], static_cast<unsigned>(event.size()));
    }
    g_event_queue.push_back(std::move(event));
    deliver_events();
}

void command_complete(uint16_t opcode, const std::vector<uint8_t>& parameters) {
    std::vector<uint8_t> event = {kEventCommandComplete, static_cast<uint8_t>(3 + parameters.size()), 1};
    put16(event, opcode);
    event.insert(event.end(), parameters.begin(), parameters.end());
    queue_event(std::move(event));
}

void command_status(uint16_t opcode, uint8_t status) {
    std::vector<uint8_t> event = {kEventCommandStatus, 4, status, 1};
    put16(event, opcode);
    queue_event(std::move(event));
}

void execute_command(uint32_t data) {
    uint16_t opcode = static_cast<uint16_t>(rd8(data) | (rd8(data + 1) << 8));
    uint32_t parameters = data + 3;
    if (g_log) {
        std::fprintf(stderr, "BT command %04x\n", opcode);
    }
    switch (opcode) {
    case kCommandReadLocalVersion: {
        std::vector<uint8_t> reply = {0x00, 0x03};
        put16(reply, 0x40a7);
        reply.push_back(0x03);
        put16(reply, 0x000F);
        put16(reply, 0x430e);
        command_complete(opcode, reply);
        break;
    }
    case kCommandReadLocalFeatures:
        command_complete(opcode, {0x00, 0xFF, 0xFF, 0x8D, 0xFE, 0x9B, 0xF9, 0x00, 0x80});
        break;
    case kCommandReadBufferSize: {
        std::vector<uint8_t> reply = {0x00};
        put16(reply, kAclPacketSize);
        reply.push_back(kScoPacketSize);
        put16(reply, kAclPacketCount);
        put16(reply, kScoPacketCount);
        command_complete(opcode, reply);
        break;
    }
    case kCommandReadBdAddr: {
        std::vector<uint8_t> reply = {0x00};
        reply.insert(reply.end(), kControllerAddress, kControllerAddress + 6);
        command_complete(opcode, reply);
        break;
    }
    case kCommandReadStoredLinkKey: {
        std::vector<uint8_t> notification = {kEventReturnLinkKeys, 1, 0};
        queue_event(std::move(notification));
        std::vector<uint8_t> reply = {0x00};
        put16(reply, 255);
        put16(reply, 0);
        command_complete(opcode, reply);
        break;
    }
    case kCommandDeleteStoredLinkKey: {
        std::vector<uint8_t> reply = {0x00};
        put16(reply, 0);
        command_complete(opcode, reply);
        break;
    }
    case kCommandWriteScanEnable:
        g_scan_enable = rd8(parameters);
        command_complete(opcode, {0x00});
        break;
    case kCommandWriteLinkSupervisionTimeout: {
        std::vector<uint8_t> reply = {0x00, rd8(parameters), rd8(parameters + 1)};
        command_complete(opcode, reply);
        break;
    }
    case kCommandInquiry:
        command_status(opcode, 0x00);
        break;
    case kCommandReset:
    default:
        command_complete(opcode, {0x00});
        break;
    }
}

}

bool handles(const std::string& path) {
    return path == kDevicePath;
}

int32_t ioctlv(uint32_t request, uint32_t command, uint32_t input_count, uint32_t, uint32_t vectors) {
    switch (command) {
    case kControlMessage: {
        uint32_t data = io_vector(vectors, input_count, 0);
        uint32_t length = rd8(in_vector(vectors, 4)) | (rd8(in_vector(vectors, 4) + 1) << 8);
        execute_command(data);
        return static_cast<int32_t>(length);
    }
    case kBulkMessage: {
        uint8_t endpoint = rd8(in_vector(vectors, 0));
        if (endpoint == kEndpointAclIn) {
            g_acl_in = {request, io_vector(vectors, input_count, 0), io_vector_size(vectors, input_count, 0)};
            return ios::kDeferred;
        }
        if (endpoint == kEndpointAclOut) {
            return static_cast<int32_t>(rd16(in_vector(vectors, 1)));
        }
        break;
    }
    case kInterruptMessage: {
        uint8_t endpoint = rd8(in_vector(vectors, 0));
        if (endpoint == kEndpointEvents) {
            g_events = {request, io_vector(vectors, input_count, 0), io_vector_size(vectors, input_count, 0)};
            deliver_events();
            return ios::kDeferred;
        }
        break;
    }
    default:
        break;
    }
    std::fprintf(stderr, "BT unhandled ioctlv %u\n", command);
    return 0;
}

void close() {
    g_events = Endpoint{};
    g_acl_in = Endpoint{};
    g_event_queue.clear();
    g_scan_enable = 0;
}

void update() {
    deliver_events();
}

bool take_completion(uint32_t& request, int32_t& result) {
    if (g_completions.empty()) {
        return false;
    }
    request = g_completions.front().request;
    result = g_completions.front().result;
    g_completions.pop_front();
    return true;
}

}
