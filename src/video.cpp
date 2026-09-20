#include "wp/video.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#include "wp/gx.h"
#include "wp/memory.h"
#include "wp/nand.h"

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
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kStandardAspect = 4.0 / 3.0;
constexpr double kWideAspect = 16.0 / 9.0;
constexpr const char* kWindowClass = "WiiPartyRecomp";

std::mutex g_lock;
std::vector<uint32_t> g_pixels;
uint32_t g_width = 0;
uint32_t g_height = 0;
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
            int target_width = client.right;
            int target_height = static_cast<int>(target_width / g_aspect);
            if (target_height > client.bottom) {
                target_height = client.bottom;
                target_width = static_cast<int>(target_height * g_aspect);
            }
            int left = (client.right - target_width) / 2;
            int top = (client.bottom - target_height) / 2;
            HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
            RECT bars[4] = {{0, 0, client.right, top},
                            {0, top + target_height, client.right, client.bottom},
                            {0, top, left, top + target_height},
                            {left + target_width, top, client.right, top + target_height}};
            for (const RECT& bar : bars) {
                FillRect(dc, &bar, black);
            }
            SetStretchBltMode(dc, HALFTONE);
            SetBrushOrgEx(dc, 0, 0, nullptr);
            StretchDIBits(dc, left, top, target_width, target_height, 0, 0, static_cast<int>(g_width),
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
    g_aspect = nand::widescreen() ? kWideAspect : kStandardAspect;
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
    uint32_t pitch = (picture & 0xFF) * 16;
    if (pitch == 0) {
        pitch = width * 2;
    }
    uint32_t copied_width = 0;
    uint32_t copied_height = 0;
    gx::last_framebuffer_size(copied_width, copied_height);
    if (copied_width != 0 && copied_width < width) {
        width = copied_width;
    }
    if (copied_height != 0 && copied_height < height) {
        height = copied_height;
    }
    uint32_t source = framebuffer_address(rd32(kTopFramebuffer));
    {
        std::lock_guard<std::mutex> guard(g_lock);
        g_width = width;
        g_height = height;
        g_pixels.resize(static_cast<size_t>(width) * height);
        for (uint32_t row = 0; row < height; row++) {
            const uint8_t* line = host(source + row * pitch);
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
    static int frames = 0;
    const char* save = std::getenv("WP_SAVE_FRAME");
    if (save && ++frames % 100 == 0) {
        std::lock_guard<std::mutex> guard(g_lock);
        save_png(save, g_pixels, g_width, g_height);
    }
    InvalidateRect(window, nullptr, FALSE);
}

}
