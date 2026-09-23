#include "wp/audio.h"

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "wp/memory.h"

namespace wp::audio {

namespace {

constexpr uint32_t kControl = 0xCD006C00;
constexpr uint32_t kSampleCounter = 0xCD006C08;
constexpr uint32_t kPlaying = 0x01;
constexpr uint32_t kSampleRate48k = 0x02;
constexpr uint32_t kCounterReset = 0x20;
constexpr double kRate32k = 32000.0;
constexpr double kRate48k = 48000.0;
constexpr double kTargetLatency = 0.06;
constexpr double kMaxLatency = 0.2;
constexpr REFERENCE_TIME kBufferDuration = 400000;

using Clock = std::chrono::steady_clock;

Clock::time_point g_last = Clock::now();
double g_samples = 0.0;

struct Frame {
    float left;
    float right;
};

std::mutex g_queue_mutex;
std::deque<Frame> g_queue;
uint32_t g_source_rate = 32000;
bool g_muted = std::getenv("WP_MUTE") != nullptr;
std::FILE* g_dump = nullptr;
uint32_t g_dump_frames = 0;

void write_wav_header(std::FILE* file, uint32_t frames, uint32_t rate) {
    uint32_t data_bytes = frames * 4;
    uint32_t values[] = {0x46464952, 36 + data_bytes, 0x45564157, 0x20746D66, 16, 0x00020001, rate, rate * 4, 0x00100004, 0x61746164, data_bytes};
    std::fseek(file, 0, SEEK_SET);
    std::fwrite(values, 4, 11, file);
    std::fseek(file, 0, SEEK_END);
    std::fflush(file);
}

template <typename T>
void release(T*& object) {
    if (object) {
        object->Release();
        object = nullptr;
    }
}

bool is_float(const WAVEFORMATEX* format) {
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        static const GUID kFloat = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        return IsEqualGUID(extensible->SubFormat, kFloat) != 0;
    }
    return false;
}

void output_thread() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    WAVEFORMATEX* format = nullptr;
    HANDLE event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    static const GUID kEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
    static const GUID kEnumeratorInterface = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
    static const GUID kClientInterface = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
    static const GUID kRenderInterface = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};
    bool ok = SUCCEEDED(CoCreateInstance(kEnumerator, nullptr, CLSCTX_ALL, kEnumeratorInterface, reinterpret_cast<void**>(&enumerator))) &&
              SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
              SUCCEEDED(device->Activate(kClientInterface, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) &&
              SUCCEEDED(client->GetMixFormat(&format)) &&
              SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, kBufferDuration, 0, format, nullptr)) &&
              SUCCEEDED(client->SetEventHandle(event)) &&
              SUCCEEDED(client->GetService(kRenderInterface, reinterpret_cast<void**>(&render)));
    UINT32 buffer_frames = 0;
    ok = ok && SUCCEEDED(client->GetBufferSize(&buffer_frames)) && SUCCEEDED(client->Start());
    if (!ok) {
        std::fprintf(stderr, "audio output could not start\n");
        release(render);
        release(client);
        release(device);
        release(enumerator);
        return;
    }
    const bool floats = is_float(format);
    const uint32_t channels = format->nChannels;
    const double device_rate = format->nSamplesPerSec;
    double position = 0.0;
    Frame previous{0.0f, 0.0f};
    Frame current{0.0f, 0.0f};
    while (true) {
        WaitForSingleObject(event, 100);
        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding))) {
            continue;
        }
        UINT32 available = buffer_frames - padding;
        if (available == 0) {
            continue;
        }
        BYTE* data = nullptr;
        if (FAILED(render->GetBuffer(available, &data))) {
            continue;
        }
        std::vector<Frame> mixed(available);
        {
            std::lock_guard<std::mutex> lock(g_queue_mutex);
            double step = g_source_rate / device_rate;
            size_t limit = static_cast<size_t>(kMaxLatency * g_source_rate);
            size_t target = static_cast<size_t>(kTargetLatency * g_source_rate);
            if (g_queue.size() > limit) {
                g_queue.erase(g_queue.begin(), g_queue.begin() + static_cast<std::ptrdiff_t>(g_queue.size() - target));
            }
            for (UINT32 i = 0; i < available; i++) {
                position += step;
                while (position >= 1.0) {
                    position -= 1.0;
                    previous = current;
                    if (!g_queue.empty()) {
                        current = g_queue.front();
                        g_queue.pop_front();
                    }
                }
                float t = static_cast<float>(position);
                mixed[i] = {previous.left + (current.left - previous.left) * t, previous.right + (current.right - previous.right) * t};
            }
        }
        for (UINT32 i = 0; i < available; i++) {
            float left = g_muted ? 0.0f : mixed[i].left;
            float right = g_muted ? 0.0f : mixed[i].right;
            for (uint32_t channel = 0; channel < channels; channel++) {
                float value = channel == 0 ? left : channel == 1 ? right : 0.0f;
                if (floats) {
                    reinterpret_cast<float*>(data)[i * channels + channel] = value;
                } else {
                    reinterpret_cast<int16_t*>(data)[i * channels + channel] = static_cast<int16_t>(std::lround(value * 32767.0f));
                }
            }
        }
        render->ReleaseBuffer(available, 0);
    }
}

}

void update() {
    Clock::time_point now = Clock::now();
    double elapsed = std::chrono::duration<double>(now - g_last).count();
    g_last = now;
    uint32_t control = rd32(kControl);
    if (control & kCounterReset) {
        g_samples = 0.0;
        wr32(kControl, control & ~kCounterReset);
        control &= ~kCounterReset;
    }
    if (control & kPlaying) {
        g_samples += elapsed * ((control & kSampleRate48k) ? kRate48k : kRate32k);
    }
    wr32(kSampleCounter, static_cast<uint32_t>(g_samples) & 0x7FFFFFFF);
}

void start_output() {
    std::thread(output_thread).detach();
}

void push(const int16_t* frames, size_t count, uint32_t rate) {
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    g_source_rate = rate;
    static const char* dump_path = std::getenv("WP_DUMP_AUDIO");
    if (dump_path && !g_dump) {
        g_dump = std::fopen(dump_path, "wb");
        if (g_dump) {
            write_wav_header(g_dump, 0, rate);
        }
    }
    if (g_dump) {
        std::fwrite(frames, 4, count, g_dump);
        g_dump_frames += static_cast<uint32_t>(count);
        if (g_dump_frames % 16000 < count) {
            write_wav_header(g_dump, g_dump_frames, rate);
        }
    }
    for (size_t i = 0; i < count; i++) {
        g_queue.push_back({frames[2 * i] / 32768.0f, frames[2 * i + 1] / 32768.0f});
    }
}

}
