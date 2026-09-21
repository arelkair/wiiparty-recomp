#include "wp/ios.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>

#include "wp/disc.h"
#include "wp/memory.h"
#include "wp/nand.h"

namespace wp::ios {

namespace {

constexpr uint32_t kTitleIdHigh = 0x00010000;
constexpr uint32_t kGameIdAddress = 0x80000000;
constexpr uint32_t kEsGetTitleId = 0x20;
constexpr uint32_t kVectorSize = 8;

constexpr uint32_t kOpen = 1;
constexpr uint32_t kClose = 2;
constexpr uint32_t kRead = 3;
constexpr uint32_t kWrite = 4;
constexpr uint32_t kSeek = 5;
constexpr uint32_t kIoctl = 6;
constexpr uint32_t kIoctlv = 7;

constexpr uint32_t kDiInquiry = 0x12;
constexpr uint32_t kDiRead = 0x71;
constexpr uint32_t kDiUnencryptedRead = 0x8D;
constexpr uint32_t kDiReportKey = 0xA4;
constexpr uint32_t kDiRequestError = 0xE0;
constexpr uint32_t kDiSuccess = 1;
constexpr uint32_t kDiError = 2;
constexpr uint32_t kDiErrorOutOfRange = 0x00052100;
constexpr uint32_t kDiErrorInvalidField = 0x00053100;
constexpr uint64_t kDiscSize = 0x118240000ull;
constexpr uint32_t kDriveInfoSize = 0x20;
constexpr const char* kEventHook = "/dev/stm/eventhook";

void log_once(const char* kind, const std::string& device, uint32_t command) {
    static std::set<std::string> seen;
    char text[160];
    std::snprintf(text, sizeof text, "%s %s command %#x", kind, device.c_str(), command);
    if (seen.insert(text).second) {
        std::fprintf(stderr, "IOS %s", text);
        std::fputc(10, stderr);
    }
}


constexpr uint32_t kFsCreateDirectory = 0x03;
constexpr uint32_t kFsGetAttributes = 0x06;
constexpr uint32_t kFsDelete = 0x07;
constexpr uint32_t kFsRename = 0x08;
constexpr uint32_t kFsCreateFile = 0x09;
constexpr uint32_t kFsGetFileStats = 0x0B;
constexpr uint32_t kFsReadDirectory = 0x04;
constexpr uint32_t kFsPathOffset = 6;
constexpr uint32_t kFsPathSize = 64;
constexpr uint32_t kFsNameSize = 13;

struct Device {
    std::string path;
    int32_t file = -1;
};

std::map<int32_t, Device> g_devices;
int32_t g_next_descriptor = 1;
uint32_t g_di_last_error = 0;

std::string read_string(uint32_t address, uint32_t limit = 256) {
    std::string text;
    for (char ch; (ch = static_cast<char>(rd8(address))) != 0 && text.size() < limit; address++) {
        text.push_back(ch);
    }
    return text;
}

std::string device_name(int32_t descriptor) {
    auto it = g_devices.find(descriptor);
    return it == g_devices.end() ? "?" : it->second.path;
}

int32_t di_command(uint32_t command, uint32_t input, uint32_t output) {
    switch (command) {
    case kDiInquiry:
        std::memset(host(output), 0, kDriveInfoSize);
        return kDiSuccess;
    case kDiRead:
        disc::read(static_cast<uint64_t>(rd32(input + 8)) << 2, rd32(input + 4), output);
        return kDiSuccess;
    case kDiUnencryptedRead: {
        uint64_t end = (static_cast<uint64_t>(rd32(input + 8)) + rd32(input + 4) / 4) * 4;
        if (end > kDiscSize) {
            g_di_last_error = kDiErrorOutOfRange;
            return kDiError;
        }
        return kDiSuccess;
    }
    case kDiReportKey:
        g_di_last_error = kDiErrorInvalidField;
        return kDiError;
    case kDiRequestError:
        wr32(output, g_di_last_error);
        g_di_last_error = 0;
        return kDiSuccess;
    default:
        log_once("ioctl", "/dev/di", command);
        return kDiSuccess;
    }
}

int32_t fs_command(uint32_t command, uint32_t input, uint32_t output) {
    switch (command) {
    case kFsCreateDirectory:
        return nand::create_directory(read_string(input + kFsPathOffset, kFsPathSize));
    case kFsCreateFile:
        return nand::create_file(read_string(input + kFsPathOffset, kFsPathSize));
    case kFsDelete:
        return nand::remove(read_string(input, kFsPathSize));
    case kFsRename:
        return nand::rename(read_string(input, kFsPathSize), read_string(input + kFsPathSize, kFsPathSize));
    case kFsGetAttributes: {
        std::string path = read_string(input, kFsPathSize);
        std::memset(host(output), 0, 12);
        return nand::exists(path) ? 0 : nand::kNotFound;
    }
    default:
        log_once("ioctl", "/dev/fs", command);
        return 0;
    }
}

int32_t fs_vector_command(uint32_t command, uint32_t input_count, uint32_t vectors) {
    if (command == kFsReadDirectory) {
        std::string path = read_string(rd32(vectors), kFsPathSize);
        uint32_t names = rd32(vectors + (input_count) * kVectorSize);
        uint32_t count_address = rd32(vectors + (input_count + 1) * kVectorSize);
        uint32_t maximum = input_count > 1 ? rd32(rd32(vectors + kVectorSize)) : 0;
        uint32_t count = 0;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(nand::host_directory(path), error)) {
            if (names != 0 && (maximum == 0 || count < maximum)) {
                std::string name = entry.path().filename().string();
                name.resize(kFsNameSize - 1);
                std::memcpy(host(names + count * kFsNameSize), name.c_str(), name.size() + 1);
            }
            count++;
        }
        wr32(count_address, count);
        return 0;
    }
    log_once("ioctlv", "/dev/fs", command);
    return 0;
}

}

bool never_completes(uint32_t request) {
    auto device = g_devices.find(static_cast<int32_t>(rd32(request + 8)));
    return rd32(request) == kIoctl && device != g_devices.end() && device->second.path == kEventHook;
}

int32_t send(uint32_t request) {
    uint32_t command = rd32(request);
    int32_t descriptor = static_cast<int32_t>(rd32(request + 8));
    int32_t result = 0;
    auto device = g_devices.find(descriptor);
    switch (command) {
    case kOpen:
        result = open(read_string(rd32(request + 12)), rd32(request + 16));
        break;
    case kClose:
        if (device != g_devices.end()) {
            nand::close(device->second.file);
            g_devices.erase(device);
        }
        break;
    case kRead:
        result = device != g_devices.end() ? nand::read(device->second.file, rd32(request + 12), rd32(request + 16))
                                           : nand::kInvalid;
        break;
    case kWrite:
        result = device != g_devices.end() ? nand::write(device->second.file, rd32(request + 12), rd32(request + 16))
                                           : nand::kInvalid;
        break;
    case kSeek:
        result = device != g_devices.end()
                     ? nand::seek(device->second.file, static_cast<int32_t>(rd32(request + 12)),
                                  static_cast<int32_t>(rd32(request + 16)))
                     : nand::kInvalid;
        break;
    case kIoctl:
        result = ioctl(descriptor, rd32(request + 12), rd32(request + 16), rd32(request + 20), rd32(request + 24),
                       rd32(request + 28));
        break;
    case kIoctlv:
        result = ioctlv(descriptor, rd32(request + 12), rd32(request + 16), rd32(request + 20), rd32(request + 24));
        break;
    default:
        std::fprintf(stderr, "IOS request %u on %s\n", command, device_name(descriptor).c_str());
        break;
    }
    wr32(request + 4, static_cast<uint32_t>(result));
    return result;
}

int32_t open(const std::string& path, uint32_t mode) {
    Device device{path, -1};
    if (path.rfind("/dev/", 0) != 0) {
        int32_t file = nand::open(path, mode);
        if (file < 0) {
            std::fprintf(stderr, "IOS open %s failed (%d)\n", path.c_str(), file);
            return file;
        }
        device.file = file;
    }
    std::fprintf(stderr, "IOS open %s\n", path.c_str());
    int32_t descriptor = g_next_descriptor++;
    g_devices[descriptor] = device;
    return descriptor;
}

int32_t ioctl(int32_t descriptor, uint32_t command, uint32_t input, uint32_t, uint32_t output, uint32_t) {
    auto it = g_devices.find(descriptor);
    const std::string device = it == g_devices.end() ? "?" : it->second.path;
    if (device == "/dev/di") {
        return di_command(command, input, output);
    }
    if (device == "/dev/fs") {
        return fs_command(command, input, output);
    }
    if (it != g_devices.end() && it->second.file >= 0 && command == kFsGetFileStats) {
        int32_t position = nand::seek(it->second.file, 0, 1);
        int32_t size = nand::seek(it->second.file, 0, 2);
        nand::seek(it->second.file, position, 0);
        wr32(output, static_cast<uint32_t>(size));
        wr32(output + 4, static_cast<uint32_t>(position));
        return 0;
    }
    log_once("ioctl", device, command);
    return 0;
}

int32_t ioctlv(int32_t descriptor, uint32_t command, uint32_t input_count, uint32_t, uint32_t vectors) {
    const std::string device = device_name(descriptor);
    if (device == "/dev/es" && command == kEsGetTitleId) {
        uint32_t output = rd32(vectors + input_count * kVectorSize);
        wr32(output, kTitleIdHigh);
        wr32(output + 4, rd32(kGameIdAddress));
        return 0;
    }
    if (device == "/dev/fs") {
        return fs_vector_command(command, input_count, vectors);
    }
    log_once("ioctlv", device, command);
    return 0;
}

}
