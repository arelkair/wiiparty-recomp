#include "wp/discord_presence.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace wp::discord {

namespace {

constexpr uint32_t kHandshake = 0;
constexpr uint32_t kFrame = 1;
constexpr uint32_t kClose = 2;
constexpr uint32_t kPing = 3;
constexpr uint32_t kPong = 4;
constexpr uint32_t kMaxBody = 65536;
constexpr size_t kMaxText = 128;
constexpr int kSlots = 10;
constexpr auto kRetry = std::chrono::seconds(15);
constexpr auto kTick = std::chrono::seconds(1);
constexpr auto kSendGap = std::chrono::seconds(5);

std::mutex g_lock;
std::condition_variable g_wake;
std::thread g_thread;
bool g_running = false;
bool g_stop = false;
bool g_dirty = false;
bool g_has_activity = false;
Activity g_activity;
std::string g_application_id;

#ifdef _WIN32
HANDLE g_pipe = INVALID_HANDLE_VALUE;

bool open_slot(int slot) {
    char name[64];
    std::snprintf(name, sizeof name, "\\\\?\\pipe\\discord-ipc-%d", slot);
    HANDLE pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }
    g_pipe = pipe;
    return true;
}

void close_connection() {
    if (g_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
}

bool write_all(const void* data, size_t size) {
    const char* bytes = static_cast<const char*>(data);
    while (size > 0) {
        DWORD written = 0;
        if (!WriteFile(g_pipe, bytes, static_cast<DWORD>(size), &written, nullptr) || written == 0) {
            return false;
        }
        bytes += written;
        size -= written;
    }
    return true;
}

bool readable(int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        DWORD available = 0;
        if (!PeekNamedPipe(g_pipe, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }
        if (available > 0) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        Sleep(10);
    }
}

bool read_all(void* data, size_t size) {
    char* bytes = static_cast<char*>(data);
    while (size > 0) {
        DWORD count = 0;
        if (!ReadFile(g_pipe, bytes, static_cast<DWORD>(size), &count, nullptr) || count == 0) {
            return false;
        }
        bytes += count;
        size -= count;
    }
    return true;
}

unsigned long process_id() {
    return GetCurrentProcessId();
}
#else
int g_socket = -1;

bool open_slot(int slot) {
    std::vector<std::string> bases;
    for (const char* variable : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) {
        const char* value = std::getenv(variable);
        if (value && *value) {
            bases.push_back(value);
        }
    }
    bases.push_back("/tmp");
    for (const std::string& base : bases) {
        for (const char* folder : {"", "/snap.discord", "/app/com.discordapp.Discord"}) {
            std::string path = base + folder + "/discord-ipc-" + std::to_string(slot);
            sockaddr_un address{};
            address.sun_family = AF_UNIX;
            if (path.size() >= sizeof address.sun_path) {
                continue;
            }
            std::memcpy(address.sun_path, path.c_str(), path.size());
            int descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
            if (descriptor < 0) {
                continue;
            }
            if (connect(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof address) == 0) {
                g_socket = descriptor;
                return true;
            }
            close(descriptor);
        }
    }
    return false;
}

void close_connection() {
    if (g_socket >= 0) {
        close(g_socket);
        g_socket = -1;
    }
}

bool write_all(const void* data, size_t size) {
    const char* bytes = static_cast<const char*>(data);
    while (size > 0) {
        ssize_t written = send(g_socket, bytes, size, MSG_NOSIGNAL);
        if (written <= 0) {
            return false;
        }
        bytes += written;
        size -= static_cast<size_t>(written);
    }
    return true;
}

bool readable(int timeout_ms) {
    pollfd descriptor{g_socket, POLLIN, 0};
    int result = poll(&descriptor, 1, timeout_ms);
    return result > 0 && (descriptor.revents & POLLIN) != 0;
}

bool read_all(void* data, size_t size) {
    char* bytes = static_cast<char*>(data);
    while (size > 0) {
        ssize_t count = recv(g_socket, bytes, size, 0);
        if (count <= 0) {
            return false;
        }
        bytes += count;
        size -= static_cast<size_t>(count);
    }
    return true;
}

unsigned long process_id() {
    return static_cast<unsigned long>(getpid());
}
#endif

void put32(unsigned char* target, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        target[i] = static_cast<unsigned char>((value >> (8 * i)) & 0xFF);
    }
}

uint32_t get32(const unsigned char* source) {
    uint32_t value = 0;
    for (int i = 0; i < 4; i++) {
        value |= static_cast<uint32_t>(source[i]) << (8 * i);
    }
    return value;
}

bool send_frame(uint32_t opcode, const std::string& body) {
    std::vector<unsigned char> packet(8 + body.size());
    put32(packet.data(), opcode);
    put32(packet.data() + 4, static_cast<uint32_t>(body.size()));
    std::memcpy(packet.data() + 8, body.data(), body.size());
    return write_all(packet.data(), packet.size());
}

bool read_frame(uint32_t& opcode, std::string& body, int timeout_ms) {
    if (!readable(timeout_ms)) {
        return false;
    }
    unsigned char header[8];
    if (!read_all(header, sizeof header)) {
        return false;
    }
    opcode = get32(header);
    uint32_t length = get32(header + 4);
    if (length > kMaxBody) {
        return false;
    }
    body.assign(length, '\0');
    return length == 0 || read_all(body.data(), length);
}

std::string clip(const std::string& text) {
    if (text.size() <= kMaxText) {
        return text;
    }
    size_t cut = kMaxText;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
        cut--;
    }
    return text.substr(0, cut);
}

std::string json_string(const std::string& text) {
    std::string result = "\"";
    for (unsigned char c : text) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (c < 0x20) {
                char escape[8];
                std::snprintf(escape, sizeof escape, "\\u%04x", c);
                result += escape;
            } else {
                result += static_cast<char>(c);
            }
        }
    }
    result += '"';
    return result;
}

std::string activity_json(const Activity& activity) {
    std::string json = "{";
    auto field = [&](const char* name, const std::string& value) {
        if (value.empty()) {
            return;
        }
        if (json.size() > 1) {
            json += ',';
        }
        json += json_string(name) + ":" + json_string(clip(value));
    };
    field("details", activity.details);
    field("state", activity.state);
    if (activity.start > 0) {
        json += (json.size() > 1 ? "," : "") + std::string("\"timestamps\":{\"start\":") + std::to_string(activity.start) + "}";
    }
    if (!activity.large_image.empty()) {
        json += (json.size() > 1 ? "," : "") + std::string("\"assets\":{\"large_image\":") + json_string(activity.large_image);
        if (!activity.large_text.empty()) {
            json += ",\"large_text\":" + json_string(clip(activity.large_text));
        }
        json += "}";
    }
    std::string buttons;
    auto button = [&](const std::string& label, const std::string& url) {
        if (label.empty() || url.empty()) {
            return;
        }
        buttons += (buttons.empty() ? "" : ",") + std::string("{\"label\":") + json_string(label.substr(0, 32)) + ",\"url\":" + json_string(url.substr(0, 512)) + "}";
    };
    button(activity.first_label, activity.first_url);
    button(activity.second_label, activity.second_url);
    if (!buttons.empty()) {
        json += (json.size() > 1 ? "," : "") + std::string("\"buttons\":[") + buttons + "]";
    }
    return json + "}";
}

int g_nonce = 0;

bool send_activity(const Activity* activity) {
    std::string arguments = "{\"pid\":" + std::to_string(process_id());
    if (activity) {
        arguments += ",\"activity\":" + activity_json(*activity);
    }
    arguments += "}";
    std::string body = "{\"cmd\":\"SET_ACTIVITY\",\"args\":" + arguments + ",\"nonce\":\"" + std::to_string(++g_nonce) + "\"}";
    if (!send_frame(kFrame, body)) {
        return false;
    }
    uint32_t opcode = 0;
    std::string reply;
    read_frame(opcode, reply, 500);
    return true;
}

bool handshake() {
    for (int slot = 0; slot < kSlots; slot++) {
        if (!open_slot(slot)) {
            continue;
        }
        uint32_t opcode = 0;
        std::string reply;
        if (send_frame(kHandshake, "{\"v\":1,\"client_id\":" + json_string(g_application_id) + "}") && read_frame(opcode, reply, 3000) &&
            opcode == kFrame && reply.find("READY") != std::string::npos) {
            return true;
        }
        close_connection();
    }
    return false;
}

bool drain() {
    uint32_t opcode = 0;
    std::string body;
    while (readable(0)) {
        if (!read_frame(opcode, body, 200)) {
            return false;
        }
        if (opcode == kClose) {
            return false;
        }
        if (opcode == kPing && !send_frame(kPong, body)) {
            return false;
        }
    }
    return true;
}

void worker() {
    std::unique_lock<std::mutex> lock(g_lock);
    bool connected = false;
    auto last_sent = std::chrono::steady_clock::time_point();
    while (!g_stop) {
        if (!connected) {
            lock.unlock();
            bool ok = handshake();
            lock.lock();
            if (!ok) {
                g_wake.wait_for(lock, kRetry, [] { return g_stop; });
                continue;
            }
            connected = true;
            g_dirty = g_has_activity;
        }
        if (g_dirty && last_sent.time_since_epoch().count() != 0) {
            auto ready = last_sent + kSendGap;
            if (std::chrono::steady_clock::now() < ready) {
                g_wake.wait_until(lock, ready, [] { return g_stop; });
                continue;
            }
        }
        if (g_dirty) {
            Activity activity = g_activity;
            bool has = g_has_activity;
            g_dirty = false;
            lock.unlock();
            bool ok = send_activity(has ? &activity : nullptr);
            lock.lock();
            last_sent = std::chrono::steady_clock::now();
            if (!ok) {
                close_connection();
                connected = false;
                continue;
            }
        }
        g_wake.wait_for(lock, kTick, [] { return g_stop || g_dirty; });
        lock.unlock();
        bool alive = drain();
        lock.lock();
        if (!alive) {
            close_connection();
            connected = false;
        }
    }
    if (connected) {
        lock.unlock();
        send_activity(nullptr);
        close_connection();
        lock.lock();
    }
}

}

void start(const char* application_id) {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_running || !application_id || !*application_id) {
        return;
    }
    g_application_id = application_id;
    g_stop = false;
    g_running = true;
    g_thread = std::thread(worker);
}

void update(const Activity& activity) {
    {
        std::lock_guard<std::mutex> guard(g_lock);
        g_activity = activity;
        g_has_activity = true;
        g_dirty = true;
    }
    g_wake.notify_all();
}

void stop() {
    {
        std::lock_guard<std::mutex> guard(g_lock);
        if (!g_running) {
            return;
        }
        g_stop = true;
        g_has_activity = false;
    }
    g_wake.notify_all();
    if (g_thread.joinable()) {
        g_thread.join();
    }
    std::lock_guard<std::mutex> guard(g_lock);
    g_running = false;
}

bool running() {
    std::lock_guard<std::mutex> guard(g_lock);
    return g_running;
}

}
