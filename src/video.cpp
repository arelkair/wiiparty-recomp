#include "wp/video.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "wp/gx.h"
#include "wp/gx_render.h"
#include "wp/memory.h"
#include "wp/nand.h"

namespace wp::video {

namespace {

constexpr uint32_t kDisplayConfig = 0xCC002002;
constexpr uint32_t kGameCodeAddress = 0x80000000;
constexpr uint32_t kGameCodeLength = 6;
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kStandardAspect = 4.0 / 3.0;
constexpr double kWideAspect = 16.0 / 9.0;
constexpr const char* kWindowClass = "WiiPartyRecomp";

std::atomic<HWND> g_window{nullptr};
double g_aspect = kStandardAspect;

uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

void put32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>(value >> shift));
    }
}

void put_chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& body) {
    put32(out, static_cast<uint32_t>(body.size()));
    std::vector<uint8_t> tagged(type, type + 4);
    tagged.insert(tagged.end(), body.begin(), body.end());
    out.insert(out.end(), tagged.begin(), tagged.end());
    put32(out, ~crc32(tagged.data(), tagged.size()));
}

void save_png(const char* path, const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height) {
    std::vector<uint8_t> raw;
    for (uint32_t y = 0; y < height; y++) {
        raw.push_back(0);
        for (uint32_t x = 0; x < width; x++) {
            uint32_t p = pixels[static_cast<size_t>(y) * width + x];
            raw.push_back(static_cast<uint8_t>(p >> 16));
            raw.push_back(static_cast<uint8_t>(p >> 8));
            raw.push_back(static_cast<uint8_t>(p));
        }
    }
    std::vector<uint8_t> deflated = {0x78, 0x01};
    size_t position = 0;
    while (position < raw.size()) {
        size_t length = std::min<size_t>(65535, raw.size() - position);
        deflated.push_back(position + length >= raw.size() ? 1 : 0);
        deflated.push_back(static_cast<uint8_t>(length));
        deflated.push_back(static_cast<uint8_t>(length >> 8));
        deflated.push_back(static_cast<uint8_t>(~length));
        deflated.push_back(static_cast<uint8_t>((~length) >> 8));
        deflated.insert(deflated.end(), raw.begin() + static_cast<std::ptrdiff_t>(position), raw.begin() + static_cast<std::ptrdiff_t>(position + length));
        position += length;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    put32(deflated, (b << 16) | a);
    std::vector<uint8_t> file = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> header;
    put32(header, width);
    put32(header, height);
    header.insert(header.end(), {8, 2, 0, 0, 0});
    put_chunk(file, "IHDR", header);
    put_chunk(file, "IDAT", deflated);
    put_chunk(file, "IEND", {});
    std::FILE* handle = std::fopen(path, "wb");
    if (handle) {
        std::fwrite(file.data(), 1, file.size(), handle);
        std::fclose(handle);
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE:
    case WM_DESTROY:
        std::_Exit(0);
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProc(window, message, wparam, lparam);
    }
}

std::string game_code() {
    std::string code;
    for (uint32_t i = 0; i < kGameCodeLength; i++) {
        char ch = static_cast<char>(rd8(kGameCodeAddress + i));
        if (ch != 0) {
            code.push_back(ch);
        }
    }
    return code;
}

std::string window_title() {
    bool pal = (rd16(kDisplayConfig) & 0x0300) == 0x0100;
    return std::string("Wii Party (") + game_code() + ")  |  Build " + WP_BUILD + "  |  " + gx::render::api_name() + "  |  " +
           (pal ? "PAL" : "NTSC");
}

void window_thread() {
    WNDCLASSA window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClass;
    RegisterClassA(&window_class);
    std::string title = window_title();
    RECT rect{0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowA(kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                                CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                                window_class.hInstance, nullptr);
    g_window = window;
    MSG message;
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

}

void start() {
    if (std::getenv("WP_HEADLESS")) {
        return;
    }
    g_aspect = nand::widescreen() ? kWideAspect : kStandardAspect;
    std::thread(window_thread).detach();
}

void* window_handle() {
    return g_window.load();
}

void present() {
    gx::process();
    HWND window = g_window;
    if (!window) {
        return;
    }
    if (!gx::render::present_frame(window, g_aspect)) {
        return;
    }
    static int frames = 0;
    const char* save = std::getenv("WP_SAVE_FRAME");
    if (save && ++frames % 100 == 0) {
        std::vector<uint32_t> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
        if (gx::render::read_frame(pixels, width, height)) {
            char path[512];
            std::snprintf(path, sizeof path, save, frames / 100);
            save_png(path, pixels, width, height);
        }
    }
}

}
