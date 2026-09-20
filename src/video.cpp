#include "wp/video.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#include "wp/gx.h"
#include "wp/memory.h"

namespace wp::video {

namespace {

constexpr uint32_t kVerticalTiming = 0xCC002000;
constexpr uint32_t kDisplayConfig = 0xCC002002;
constexpr uint32_t kTopFramebuffer = 0xCC00201C;
constexpr uint32_t kPictureConfig = 0xCC002048;
constexpr uint32_t kFramebufferBase = 0x80000000;
constexpr uint32_t kMaxWidth = 720;
constexpr uint32_t kMaxHeight = 576;
constexpr uint32_t kDefaultHeight = 480;
constexpr int kWindowWidth = 640;
constexpr int kWindowHeight = 480;
constexpr const char* kWindowClass = "WiiPartyRecomp";

std::mutex g_lock;
std::vector<uint32_t> g_pixels;
uint32_t g_width = 0;
uint32_t g_height = 0;
std::atomic<HWND> g_window{nullptr};

uint8_t clamp(int value) {
    return static_cast<uint8_t>(value < 0 ? 0 : value > 255 ? 255 : value);
}

uint32_t to_rgb(int y, int u, int v) {
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;
    uint8_t r = clamp((298 * c + 409 * e + 128) >> 8);
    uint8_t g = clamp((298 * c - 100 * d - 208 * e + 128) >> 8);
    uint8_t b = clamp((298 * c + 516 * d + 128) >> 8);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
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
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);
        std::lock_guard<std::mutex> guard(g_lock);
        if (g_width != 0 && g_height != 0) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = static_cast<LONG>(g_width);
            info.bmiHeader.biHeight = -static_cast<LONG>(g_height);
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            SetStretchBltMode(dc, HALFTONE);
            StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0, static_cast<int>(g_width),
                          static_cast<int>(g_height), g_pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
        } else {
            FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        }
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProc(window, message, wparam, lparam);
    }
}

void window_thread() {
    WNDCLASSA window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClass;
    RegisterClassA(&window_class);
    RECT rect{0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowA(kWindowClass, "Wii Party", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                                CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                                window_class.hInstance, nullptr);
    g_window = window;
    MSG message;
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

uint32_t framebuffer_address(uint32_t reg) {
    uint32_t address = reg & 0x00FFFFFF;
    if (reg & 0x10000000) {
        address <<= 5;
    }
    return kFramebufferBase | address;
}

}

void start() {
    if (std::getenv("WP_HEADLESS")) {
        return;
    }
    std::thread(window_thread).detach();
}

void present() {
    gx::process();
    HWND window = g_window;
    if (!window) {
        return;
    }
    uint16_t config = rd16(kDisplayConfig);
    uint32_t picture = rd16(kPictureConfig);
    uint32_t width = ((picture >> 8) & 0x7F) * 16;
    uint32_t lines = (rd16(kVerticalTiming) >> 4) & 0x3FF;
    uint32_t height = lines == 0 ? kDefaultHeight : (config & 4) ? lines : lines * 2;
    if (!(config & 1) || width == 0 || width > kMaxWidth || height > kMaxHeight) {
        return;
    }
    uint32_t source = framebuffer_address(rd32(kTopFramebuffer));
    {
        std::lock_guard<std::mutex> guard(g_lock);
        g_width = width;
        g_height = height;
        g_pixels.resize(static_cast<size_t>(width) * height);
        for (uint32_t row = 0; row < height; row++) {
            const uint8_t* line = host(source + row * (width * 2));
            uint32_t* out = &g_pixels[static_cast<size_t>(row) * width];
            for (uint32_t x = 0; x < width; x += 2) {
                int y0 = line[2 * x];
                int u = line[2 * x + 1];
                int y1 = line[2 * x + 2];
                int v = line[2 * x + 3];
                out[x] = to_rgb(y0, u, v);
                out[x + 1] = to_rgb(y1, u, v);
            }
        }
    }
    InvalidateRect(window, nullptr, FALSE);
}

}
