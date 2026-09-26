#include "wp/video.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "wp/dsp.h"
#include "wp/game.h"
#include "wp/gx.h"
#include "wp/gx_render.h"
#include "wp/input.h"
#include "wp/keymap.h"
#include "wp/log.h"
#include "wp/memory.h"
#include "wp/nand.h"
#include "wp/screenshot.h"
#include "wp/settings.h"
#include "wp/ui_text.h"

namespace wp::video {

namespace {

constexpr uint32_t kDisplayConfig = 0xCC002002;
constexpr uint32_t kGameCodeAddress = 0x80000000;
constexpr uint32_t kGameCodeLength = 6;
constexpr int kWindowWidth = 1280;
constexpr double kStandardAspect = 4.0 / 3.0;
constexpr double kWideAspect = 16.0 / 9.0;
constexpr const char* kWindowClass = "WiiRecompWindow";
constexpr UINT kTitleMessage = WM_APP + 1;

std::atomic<HWND> g_window{nullptr};
std::atomic<bool> g_screenshot_requested{false};
double g_aspect = kStandardAspect;
std::mutex g_title_mutex;
std::string g_title;

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


bool hide_cursor() {
    static const bool value = settings::flag("input.hide_cursor", "WP_HIDE_CURSOR");
    return value;
}

WINDOWPLACEMENT g_placement{};
bool g_fullscreen = false;

void set_fullscreen(HWND window, bool enable) {
    LONG style = GetWindowLong(window, GWL_STYLE);
    if (enable) {
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(MONITORINFO);
        g_placement.length = sizeof(WINDOWPLACEMENT);
        if (!GetWindowPlacement(window, &g_placement) || !GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
            return;
        }
        SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top, monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &g_placement);
        SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    g_fullscreen = enable;
}

bool bound_key(WPARAM key) {
    const keymap::Bindings& keys = input::bindings();
    int code = static_cast<int>(key);
    switch (key) {
    case VK_SHIFT:
        return keys.bound(code) || keys.bound(VK_LSHIFT) || keys.bound(VK_RSHIFT);
    case VK_CONTROL:
        return keys.bound(code) || keys.bound(VK_LCONTROL) || keys.bound(VK_RCONTROL);
    case VK_MENU:
        return keys.bound(code) || keys.bound(VK_LMENU) || keys.bound(VK_RMENU);
    default:
        return keys.bound(code);
    }
}

int side_key(WPARAM key, LPARAM lparam) {
    bool extended = (lparam & (1 << 24)) != 0;
    switch (key) {
    case VK_SHIFT:
        return static_cast<int>(MapVirtualKeyA((lparam >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX));
    case VK_CONTROL:
        return extended ? VK_RCONTROL : VK_LCONTROL;
    case VK_MENU:
        return extended ? VK_RMENU : VK_LMENU;
    default:
        return static_cast<int>(key);
    }
}

void note_press(int code, int side = 0) {
    const keymap::Bindings& keys = input::bindings();
    if (keys.has(keymap::Action::Screenshot, code) || (side != 0 && keys.has(keymap::Action::Screenshot, side))) {
        g_screenshot_requested = true;
    }
}

void note_key(WPARAM key, LPARAM lparam) {
    if (!(lparam & (1 << 30))) {
        note_press(static_cast<int>(key), side_key(key, lparam));
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE:
    case WM_DESTROY:
        std::_Exit(0);
    case kTitleMessage: {
        std::string title;
        {
            std::lock_guard<std::mutex> lock(g_title_mutex);
            title = g_title;
        }
        SetWindowTextA(window, title.c_str());
        return 0;
    }
    case WM_KEYDOWN:
        if (wparam == VK_F12 && !(lparam & (1 << 30))) {
            gx::request_capture();
            return 0;
        }
        if (wparam == VK_F11 && !(lparam & (1 << 30))) {
            set_fullscreen(window, !g_fullscreen);
            settings::store("video.fullscreen", g_fullscreen ? "1" : "0");
            return 0;
        }
        note_key(wparam, lparam);
        return DefWindowProc(window, message, wparam, lparam);
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        if (message == WM_SYSKEYDOWN) {
            note_key(wparam, lparam);
        }
        if (wparam != VK_F4 && bound_key(wparam)) {
            return 0;
        }
        return DefWindowProc(window, message, wparam, lparam);
    case WM_LBUTTONDOWN:
        note_press(VK_LBUTTON);
        return DefWindowProc(window, message, wparam, lparam);
    case WM_RBUTTONDOWN:
        note_press(VK_RBUTTON);
        return DefWindowProc(window, message, wparam, lparam);
    case WM_MBUTTONDOWN:
        note_press(VK_MBUTTON);
        return DefWindowProc(window, message, wparam, lparam);
    case WM_XBUTTONDOWN:
        note_press(GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
        return TRUE;
    case WM_SETCURSOR:
        if (LOWORD(lparam) == HTCLIENT && hide_cursor()) {
            SetCursor(nullptr);
            return TRUE;
        }
        return DefWindowProc(window, message, wparam, lparam);
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wparam);
        input::note_wheel(delta);
        if (delta != 0) {
            note_press(delta > 0 ? keymap::kWheelUp : keymap::kWheelDown);
        }
        return 0;
    }
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

std::string window_title(double fps) {
    const ui::Text& text = ui::text();
    bool pal = (rd16(kDisplayConfig) & 0x0300) == 0x0100;
    std::string code = game_code();
    bool pal_disc = code.size() >= 4 && code[3] == 'P';
    const char* region = pal ? "PAL" : pal_disc ? "PAL60" : "NTSC";
    dsp::Status status = dsp::status();
    char statistics[128];
    if (status.loaded) {
        std::snprintf(statistics, sizeof statistics, "%s: %.0f | %s: %s (%.0f%%)", text.fps, fps, text.dsp,
                      status.native ? text.dsp_native : text.dsp_interpreted, status.load_percent);
    } else {
        std::snprintf(statistics, sizeof statistics, "%s: %.0f | %s: %s", text.fps, fps, text.dsp, text.dsp_stopped);
    }
    return std::string(game::description().title) + " (" + code + ") | " + gx::render::api_name() + " | " + region + " | " +
           statistics;
}

void window_thread() {
    WNDCLASSA window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIcon(window_class.hInstance, MAKEINTRESOURCE(1));
    window_class.lpszClassName = kWindowClass;
    RegisterClassA(&window_class);
    std::string title = window_title(0.0);
    RECT work;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    int client_width = kWindowWidth;
    int client_height = static_cast<int>(client_width / g_aspect + 0.5);
    RECT rect{0, 0, client_width, client_height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    while ((rect.right - rect.left > work.right - work.left || rect.bottom - rect.top > work.bottom - work.top) && client_width > 320) {
        client_width -= 16;
        client_height = static_cast<int>(client_width / g_aspect + 0.5);
        rect = {0, 0, client_width, client_height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    }
    HWND window = CreateWindowA(kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                                CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                                window_class.hInstance, nullptr);
    g_window = window;
    if (settings::flag("video.fullscreen", "WP_FULLSCREEN")) {
        set_fullscreen(window, true);
    }
    MSG message;
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

}

void start() {
    log::write("video", "build %s", WP_BUILD);
    g_aspect = nand::widescreen() ? kWideAspect : kStandardAspect;
    if (std::getenv("WP_HEADLESS")) {
        return;
    }
    std::thread(window_thread).detach();
}

void* window_handle() {
    return g_window.load();
}

bool image_point(long x, long y, long client_width, long client_height, float& out_x, float& out_y) {
    if (client_width <= 0 || client_height <= 0) {
        return false;
    }
    double width = client_width;
    double height = width / g_aspect;
    if (height > client_height) {
        height = client_height;
        width = height * g_aspect;
    }
    double left = (client_width - width) / 2;
    double top = (client_height - height) / 2;
    double u = (x - left) / width;
    double v = (y - top) / height;
    if (u < 0.0 || u >= 1.0 || v < 0.0 || v >= 1.0) {
        return false;
    }
    out_x = static_cast<float>(u * 2.0 - 1.0);
    out_y = static_cast<float>(v * 2.0 - 1.0);
    return true;
}

void update_statistics(double fps) {
    HWND window = g_window;
    if (!window) {
        return;
    }
    std::string title = window_title(fps);
    {
        std::lock_guard<std::mutex> lock(g_title_mutex);
        if (title == g_title) {
            return;
        }
        g_title = title;
    }
    PostMessageA(window, kTitleMessage, 0, 0);
}

std::string g_next_frame_path;

namespace {

std::tm local_time() {
    SYSTEMTIME now;
    GetLocalTime(&now);
    std::tm time{};
    time.tm_year = now.wYear - 1900;
    time.tm_mon = now.wMonth - 1;
    time.tm_mday = now.wDay;
    time.tm_hour = now.wHour;
    time.tm_min = now.wMinute;
    time.tm_sec = now.wSecond;
    return time;
}

void save_screenshot() {
    std::vector<uint32_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    if (!gx::render::read_frame(pixels, width, height)) {
        return;
    }
    CreateDirectoryA(screenshot::kFolder, nullptr);
    std::tm time = local_time();
    std::string path;
    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 1; attempt < 1000 && file == INVALID_HANDLE_VALUE; attempt++) {
        path = std::string(screenshot::kFolder) + "/" + screenshot::file_name(game::description().short_name, time, attempt);
        file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS) {
            break;
        }
    }
    if (file == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "screenshot could not be saved to %s", path.c_str());
        std::fputc(10, stderr);
        log::write("video", "screenshot could not be saved to %s", path.c_str());
        return;
    }
    std::thread([file, path, pixels = std::move(pixels), width, height] {
        std::vector<uint8_t> png = encode_png(pixels, width, height);
        DWORD written = 0;
        bool ok = WriteFile(file, png.data(), static_cast<DWORD>(png.size()), &written, nullptr) && written == png.size();
        CloseHandle(file);
        if (!ok) {
            DeleteFileA(path.c_str());
        }
        std::fprintf(stderr, ok ? "screenshot saved to %s" : "screenshot could not be written to %s", path.c_str());
        std::fputc(10, stderr);
        log::write("video", ok ? "screenshot saved to %s" : "screenshot could not be written to %s", path.c_str());
    }).detach();
}

}

void present_on_gpu(HWND window, double aspect) {
    if (window) {
        gx::render::present_frame(window, aspect);
    }
    if (g_screenshot_requested.exchange(false)) {
        save_screenshot();
    }
    if (!g_next_frame_path.empty()) {
        std::vector<uint32_t> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
        if (gx::render::read_frame(pixels, width, height)) {
            save_png(g_next_frame_path.c_str(), pixels, width, height);
        }
        g_next_frame_path.clear();
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

std::vector<uint8_t> encode_png(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height) {
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(width * 3 + 1) * height);
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
    return file;
}

void save_png(const char* path, const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height) {
    std::vector<uint8_t> file = encode_png(pixels, width, height);
    std::FILE* handle = std::fopen(path, "wb");
    if (handle) {
        std::fwrite(file.data(), 1, file.size(), handle);
        std::fclose(handle);
    }
}

void save_next_frame(const char* path) {
    g_next_frame_path = path;
}

void present() {
    HWND window = g_window;
    static const bool saving = std::getenv("WP_SAVE_FRAME") || std::getenv("WP_CAPTURE_AT");
    if (!window && !saving) {
        gx::process();
        return;
    }
    double aspect = g_aspect;
    gx::run_frame_task([window, aspect] { present_on_gpu(window, aspect); });
}

}
