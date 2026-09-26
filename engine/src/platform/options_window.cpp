#include "wp/options_window.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "wp/gamepad.h"
#include "wp/input.h"
#include "wp/settings.h"
#include "wp/ui_text.h"

namespace wp::options {

namespace {

constexpr COLORREF kPanel = RGB(22, 22, 24);
constexpr COLORREF kSelected = RGB(44, 44, 48);
constexpr COLORREF kSeparator = RGB(58, 58, 62);
constexpr COLORREF kText = RGB(255, 255, 255);
constexpr COLORREF kMuted = RGB(161, 161, 166);
constexpr COLORREF kAccent = RGB(212, 20, 111);
constexpr uint32_t kPanelAlpha = 236;
constexpr uint32_t kRepeating = gamepad::kMenuUp | gamepad::kMenuDown | gamepad::kMenuLeft | gamepad::kMenuRight;
constexpr int64_t kRepeatDelay = 400;
constexpr int64_t kRepeatInterval = 120;

Menu g_menu;
bool g_shown = false;

bool menu_enabled() {
    static const settings::LiveFlag value("system.options_menu", "WP_OPTIONS_MENU");
    return value();
}

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(std::max(0, length)), L'\0');
    if (length > 0) {
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    }
    return result;
}

HFONT make_font(int height, int weight) {
    return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void fill(HDC dc, int left, int top, int right, int bottom, COLORREF color) {
    RECT rect{left, top, right, bottom};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void text(HDC dc, HFONT font, COLORREF color, const std::wstring& value, int left, int top, int right, int bottom, UINT format) {
    SelectObject(dc, font);
    SetTextColor(dc, color);
    RECT rect{left, top, right, bottom};
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect, format | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

int text_width(HDC dc, HFONT font, const std::wstring& value) {
    SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &size);
    return size.cx;
}

uint32_t corner_alpha(int x, int y, int width, int height, int radius) {
    double cx = x + 0.5;
    double cy = y + 0.5;
    double ox = cx < radius ? radius - cx : cx > width - radius ? cx - (width - radius) : 0.0;
    double oy = cy < radius ? radius - cy : cy > height - radius ? cy - (height - radius) : 0.0;
    if (ox <= 0.0 || oy <= 0.0) {
        return kPanelAlpha;
    }
    double coverage = std::clamp(radius - std::sqrt(ox * ox + oy * oy) + 0.5, 0.0, 1.0);
    return static_cast<uint32_t>(kPanelAlpha * coverage + 0.5);
}

int client_height(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    return client.bottom;
}

void sync(void* handle) {
    HWND window = static_cast<HWND>(handle);
    if (g_menu.open()) {
        show_overlay(render(g_menu, client_height(window)));
        if (!g_shown) {
            g_shown = true;
            input::set_blocked(true);
        }
    } else if (g_shown) {
        g_shown = false;
        hide_overlay();
        input::set_blocked(false);
    }
}

std::string act(void* window, Action action) {
    std::string changed = g_menu.handle(action);
    sync(window);
    return changed;
}

}

Image render(const Menu& menu, int client_height) {
    Image image;
    std::vector<Row> rows = menu.rows();
    Layout frame = layout(client_height, rows.size());
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = frame.width;
    info.bmiHeader.biHeight = -frame.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP bitmap = dc ? CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (!dc || !bitmap || !bits) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (dc) {
            DeleteDC(dc);
        }
        return image;
    }
    HGDIOBJ previous_bitmap = SelectObject(dc, bitmap);
    int unit = frame.unit;
    HFONT title_font = make_font(unit * 3 / 2, FW_SEMIBOLD);
    HFONT row_font = make_font(unit, FW_NORMAL);
    HFONT small_font = make_font(unit * 4 / 5, FW_NORMAL);
    HGDIOBJ previous_font = SelectObject(dc, row_font);
    SetBkMode(dc, TRANSPARENT);
    const ui::Text& strings = ui::text();
    int left = frame.padding;
    int right = frame.width - frame.padding;
    fill(dc, 0, 0, frame.width, frame.height, kPanel);
    text(dc, title_font, kText, widen(strings.menu_title), left, frame.padding, right, frame.padding + frame.title_height - unit / 2, DT_LEFT);
    int rows_top = frame.padding + frame.title_height;
    fill(dc, left, rows_top - unit / 2, right, rows_top - unit / 2 + 1, kSeparator);
    int value_width = unit * 9;
    for (size_t i = 0; i < rows.size(); i++) {
        const Row& row = rows[i];
        bool selected = i == menu.selection();
        int top = rows_top + static_cast<int>(i) * frame.row_height;
        int bottom = top + frame.row_height;
        if (selected) {
            fill(dc, left - unit / 2, top, right + unit / 2, bottom, kSelected);
            fill(dc, left - unit / 2, top, left - unit / 2 + std::max(3, unit / 5), bottom, kAccent);
        }
        int value_left = right - value_width;
        text(dc, row_font, selected ? kAccent : kText, widen(row.value), value_left, top, right, bottom, DT_RIGHT | DT_NOCLIP);
        int label_right = value_left - unit / 2;
        if (!row.live) {
            std::wstring note = widen(strings.menu_restart);
            int note_left = value_left - unit / 2 - text_width(dc, small_font, note);
            text(dc, small_font, kMuted, note, note_left - unit, top, value_left - unit / 2, bottom, DT_RIGHT | DT_NOCLIP);
            label_right = note_left - unit / 2;
        }
        text(dc, row_font, kText, widen(row.label), left + unit / 2, top, std::max(left + unit, label_right), bottom, DT_LEFT | DT_END_ELLIPSIS);
    }
    int footer_top = rows_top + static_cast<int>(rows.size()) * frame.row_height;
    fill(dc, left, footer_top + unit / 2, right, footer_top + unit / 2 + 1, kSeparator);
    text(dc, small_font, kMuted, widen(strings.menu_hint), left, footer_top + unit / 2, right, footer_top + frame.footer_height, DT_CENTER | DT_END_ELLIPSIS);
    GdiFlush();
    image.width = static_cast<uint32_t>(frame.width);
    image.height = static_cast<uint32_t>(frame.height);
    image.pixels.resize(static_cast<size_t>(image.width) * image.height);
    const uint32_t* source = static_cast<const uint32_t*>(bits);
    int radius = unit * 3 / 4;
    for (int y = 0; y < frame.height; y++) {
        for (int x = 0; x < frame.width; x++) {
            size_t index = static_cast<size_t>(y) * image.width + static_cast<size_t>(x);
            image.pixels[index] = (source[index] & 0xFFFFFF) | (corner_alpha(x, y, frame.width, frame.height, radius) << 24);
        }
    }
    SelectObject(dc, previous_font);
    SelectObject(dc, previous_bitmap);
    DeleteObject(title_font);
    DeleteObject(row_font);
    DeleteObject(small_font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return image;
}

bool menu_open() {
    return g_menu.open();
}

void toggle_menu(void* window) {
    if (!g_menu.open() && !menu_enabled()) {
        return;
    }
    g_menu.set_open(!g_menu.open());
    sync(window);
}

std::string menu_key(void* window, unsigned key, bool repeat) {
    if (!g_menu.open()) {
        return "";
    }
    switch (key) {
    case VK_UP:
        return act(window, Action::Up);
    case VK_DOWN:
        return act(window, Action::Down);
    case VK_LEFT:
        return act(window, Action::Previous);
    case VK_RIGHT:
        return act(window, Action::Next);
    case VK_RETURN:
    case VK_SPACE:
        return repeat ? "" : act(window, Action::Next);
    case VK_ESCAPE:
    case VK_BACK:
        return repeat ? "" : act(window, Action::Close);
    default:
        return "";
    }
}

std::string menu_wheel(void* window, int delta) {
    if (!g_menu.open() || delta == 0) {
        return "";
    }
    return act(window, delta > 0 ? Action::Up : Action::Down);
}

std::string menu_click(void* handle, int x, int y, bool right) {
    if (!g_menu.open()) {
        return "";
    }
    if (right) {
        return act(handle, Action::Close);
    }
    HWND window = static_cast<HWND>(handle);
    RECT client{};
    GetClientRect(window, &client);
    Layout frame = layout(client.bottom, settings::keys().size());
    int row = row_at(frame, place(client.right, client.bottom, frame.width, frame.height), x, y);
    if (row < 0) {
        return "";
    }
    g_menu.select(static_cast<size_t>(row));
    return act(handle, Action::Next);
}

std::string poll_gamepads(void* window) {
    static Repeat repeat(kRepeating, kRepeatDelay, kRepeatInterval);
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    bool focused = GetForegroundWindow() == static_cast<HWND>(window);
    uint32_t fired = repeat.update(focused ? gamepad::menu_buttons() : 0, now);
    if (fired & gamepad::kMenuToggle) {
        toggle_menu(window);
        return "";
    }
    if (!g_menu.open() || fired == 0) {
        return "";
    }
    if (fired & gamepad::kMenuCancel) {
        return act(window, Action::Close);
    }
    std::string changed;
    if (fired & gamepad::kMenuUp) {
        act(window, Action::Up);
    }
    if (fired & gamepad::kMenuDown) {
        act(window, Action::Down);
    }
    if (fired & gamepad::kMenuLeft) {
        changed = act(window, Action::Previous);
    }
    if (fired & (gamepad::kMenuRight | gamepad::kMenuAccept)) {
        changed = act(window, Action::Next);
    }
    return changed;
}

void refresh_menu(void* window) {
    if (g_menu.open()) {
        sync(window);
    }
}

}
