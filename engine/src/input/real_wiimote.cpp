#include "wp/real_wiimote.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "wp/input.h"
#include "wp/settings.h"
#include "wp/wiimote.h"

namespace wp::real_wiimote {

namespace {

constexpr uint16_t kNintendo = 0x057e;
constexpr uint16_t kRemote = 0x0306;
constexpr uint16_t kRemotePlus = 0x0330;
constexpr uint32_t kSlots = 4;
constexpr size_t kLongestReport = 22;
constexpr size_t kQueueLimit = 256;
constexpr uint8_t kHidDataInput = 0xA1;
constexpr uint8_t kStatusRequest = 0x15;
constexpr uint16_t kButtonMask = 0x9F1F;
constexpr uint8_t kReportIrEnable = 0x13;
constexpr uint8_t kReportWriteMemory = 0x16;
constexpr uint32_t kCameraModeAddress = 0xB00033;
constexpr uint8_t kRegisterSpace = 0x04;

struct Remote {
    std::mutex mutex;
    SDL_hid_device* device = nullptr;
    std::string path;
    std::atomic<bool> present{false};
    std::atomic<bool> woke{false};
    std::deque<std::vector<uint8_t>> input;
    std::deque<std::vector<uint8_t>> output;
    bool camera_on = false;
    uint8_t camera_mode = 0;
};

std::array<Remote, kSlots> g_remotes;
std::once_flag g_started;

size_t payload_size(uint8_t id) {
    switch (id) {
    case 0x20:
        return 6;
    case 0x22:
        return 4;
    case 0x30:
        return 2;
    case 0x31:
        return 5;
    case 0x32:
        return 10;
    case 0x33:
        return 14;
    case 0x21:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x3d:
    case 0x3e:
    case 0x3f:
        return 21;
    default:
        return 0;
    }
}

bool pressed(const uint8_t* report, size_t size) {
    if (size < 3 || report[0] == 0x3d) {
        return false;
    }
    uint16_t buttons = static_cast<uint16_t>((report[1] << 8) | report[2]);
    return (buttons & kButtonMask) != 0;
}

void close_remote(uint32_t slot) {
    Remote& remote = g_remotes[slot];
    std::lock_guard<std::mutex> lock(remote.mutex);
    if (remote.device) {
        SDL_hid_close(remote.device);
    }
    std::fprintf(stderr, "real wiimote %u disconnected\n", slot + 1);
    remote.device = nullptr;
    remote.path.clear();
    remote.input.clear();
    remote.output.clear();
    remote.woke = false;
    remote.present = false;
}

void open_new(std::set<std::string>& refused) {
    SDL_hid_device_info* list = SDL_hid_enumerate(kNintendo, 0);
    for (SDL_hid_device_info* info = list; info; info = info->next) {
        if ((info->product_id != kRemote && info->product_id != kRemotePlus) || !info->path) {
            continue;
        }
        std::string path = info->path;
        bool known = false;
        uint32_t free_slot = kSlots;
        for (uint32_t i = 0; i < kSlots; i++) {
            if (g_remotes[i].present && g_remotes[i].path == path) {
                known = true;
            }
            if (!g_remotes[i].present && free_slot == kSlots) {
                free_slot = i;
            }
        }
        if (known || free_slot == kSlots) {
            continue;
        }
        SDL_hid_device* device = SDL_hid_open_path(info->path);
        if (!device) {
            continue;
        }
        const uint8_t probe[] = {kStatusRequest, 0x00};
        if (SDL_hid_write(device, probe, sizeof(probe)) < 0) {
            SDL_hid_close(device);
            if (refused.insert(path).second) {
                std::fprintf(stderr, "real wiimote at %s is paired but not connected\n", path.c_str());
            }
            continue;
        }
        uint8_t buffer[kLongestReport];
        while (SDL_hid_read_timeout(device, buffer, sizeof(buffer), 50) > 0) {
        }
        refused.erase(path);
        Remote& remote = g_remotes[free_slot];
        std::lock_guard<std::mutex> lock(remote.mutex);
        remote.device = device;
        remote.path = path;
        remote.input.clear();
        remote.output.clear();
        remote.woke = true;
        remote.present = true;
        std::fprintf(stderr, "real wiimote %u connected (%04x)\n", free_slot + 1, info->product_id);
    }
    SDL_hid_free_enumeration(list);
}

bool service(uint32_t slot) {
    Remote& remote = g_remotes[slot];
    if (!remote.present) {
        return false;
    }
    std::deque<std::vector<uint8_t>> pending;
    SDL_hid_device* device;
    {
        std::lock_guard<std::mutex> lock(remote.mutex);
        pending.swap(remote.output);
        device = remote.device;
    }
    for (const std::vector<uint8_t>& report : pending) {
        if (SDL_hid_write(device, report.data(), report.size()) < 0) {
            close_remote(slot);
            return false;
        }
    }
    bool busy = !pending.empty();
    uint8_t buffer[kLongestReport];
    while (true) {
        int count = SDL_hid_read_timeout(device, buffer, sizeof(buffer), 0);
        if (count < 0) {
            close_remote(slot);
            return false;
        }
        if (count == 0) {
            break;
        }
        busy = true;
        size_t size = payload_size(buffer[0]);
        if (size == 0 || static_cast<size_t>(count) < size + 1) {
            continue;
        }
        std::vector<uint8_t> frame;
        frame.reserve(size + 2);
        frame.push_back(kHidDataInput);
        frame.insert(frame.end(), buffer, buffer + size + 1);
        if (pressed(buffer, size + 1)) {
            remote.woke = true;
        }
        std::lock_guard<std::mutex> lock(remote.mutex);
        remote.input.push_back(std::move(frame));
        if (remote.input.size() > kQueueLimit) {
            remote.input.pop_front();
        }
    }
    return busy;
}

void aim_with_mouse(std::vector<uint8_t>& frame, uint8_t mode) {
    size_t offset = 0;
    size_t size = 0;
    switch (frame[1]) {
    case 0x33:
        offset = 7;
        size = 12;
        break;
    case 0x36:
        offset = 4;
        size = 10;
        break;
    case 0x37:
        offset = 7;
        size = 10;
        break;
    default:
        return;
    }
    if (frame.size() < offset + size) {
        return;
    }
    for (size_t i = 0; i < size; i++) {
        if (frame[offset + i] != 0xFF) {
            return;
        }
    }
    uint8_t objects[36];
    wiimote::camera_objects(mode, input::sample(0), objects);
    std::copy(objects, objects + size, frame.begin() + static_cast<long>(offset));
}

void run() {
    std::set<std::string> refused;
    auto next_scan = std::chrono::steady_clock::now();
    Uint32 changes = 0;
    while (true) {
        auto now = std::chrono::steady_clock::now();
        Uint32 count = SDL_hid_device_change_count();
        if (now >= next_scan || count != changes) {
            changes = count;
            open_new(refused);
            next_scan = now + std::chrono::seconds(2);
        }
        bool busy = false;
        for (uint32_t i = 0; i < kSlots; i++) {
            busy = service(i) || busy;
        }
        if (!busy) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

}

void start() {
    std::call_once(g_started, [] {
        if (!settings::flag("input.real_wiimotes", "WP_REAL_WIIMOTES") || SDL_hid_init() != 0) {
            return;
        }
        std::thread(run).detach();
    });
}

bool present(uint32_t slot) {
    return slot < kSlots && g_remotes[slot].present;
}

bool take_wake(uint32_t slot) {
    return slot < kSlots && g_remotes[slot].woke.exchange(false);
}

void reset(uint32_t slot) {
    if (slot >= kSlots) {
        return;
    }
    Remote& remote = g_remotes[slot];
    std::lock_guard<std::mutex> lock(remote.mutex);
    remote.input.clear();
}

void output_report(uint32_t slot, const uint8_t* data, uint32_t size) {
    if (slot >= kSlots || size == 0) {
        return;
    }
    Remote& remote = g_remotes[slot];
    std::lock_guard<std::mutex> lock(remote.mutex);
    if (!remote.present) {
        return;
    }
    if (data[0] == kReportIrEnable && size >= 2) {
        remote.camera_on = (data[1] & 0x04) != 0;
    } else if (data[0] == kReportWriteMemory && size >= 7 && (data[1] & kRegisterSpace)) {
        uint32_t address = static_cast<uint32_t>((data[2] << 16) | (data[3] << 8) | data[4]);
        uint8_t length = data[5];
        if (address <= kCameraModeAddress && kCameraModeAddress < address + length && 6 + (kCameraModeAddress - address) < size) {
            remote.camera_mode = data[6 + (kCameraModeAddress - address)];
        }
    }
    remote.output.emplace_back(data, data + size);
}

void update(uint32_t slot, const Sender& send) {
    if (slot >= kSlots) {
        return;
    }
    std::deque<std::vector<uint8_t>> reports;
    bool camera_on;
    uint8_t camera_mode;
    {
        Remote& remote = g_remotes[slot];
        std::lock_guard<std::mutex> lock(remote.mutex);
        reports.swap(remote.input);
        camera_on = remote.camera_on;
        camera_mode = remote.camera_mode;
    }
    static const settings::LiveFlag mouse_pointer("input.real_wiimote_mouse", "WP_REAL_WIIMOTE_MOUSE");
    bool substitute = slot == 0 && camera_on && mouse_pointer();
    for (std::vector<uint8_t>& report : reports) {
        if (substitute) {
            aim_with_mouse(report, camera_mode);
        }
        send(report);
    }
}

}
