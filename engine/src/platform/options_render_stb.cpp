#include "wp/options_window.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb_truetype.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "wp/log.h"
#include "wp/ui_text.h"

namespace wp::options {

namespace {

constexpr uint32_t kPanel = 0x161618;
constexpr uint32_t kSelected = 0x2C2C30;
constexpr uint32_t kSeparator = 0x3A3A3E;
constexpr uint32_t kText = 0xFFFFFF;
constexpr uint32_t kMuted = 0xA1A1A6;
constexpr uint32_t kAccent = 0xD4146F;
constexpr uint32_t kAccentHover = 0xE8307F;
constexpr uint32_t kHover = 0x202023;
constexpr uint32_t kControl = 0x1E1E21;
constexpr uint32_t kControlHover = 0x2A2A2E;
constexpr uint32_t kOff = 0x4A4A50;
constexpr uint32_t kOffHover = 0x5C5C63;
constexpr uint32_t kDanger = 0xC42B1C;
constexpr uint32_t kPanelAlpha = 236;
constexpr char32_t kEllipsis = 0x2026;

enum class Align { Left, Right, Center };

struct Font {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    bool ok = false;
};

std::string matched_font(const char* pattern) {
#ifdef _WIN32
    (void)pattern;
    return "";
#else
    std::string command = std::string("fc-match -f '%{file}' '") + pattern + "' 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    std::string path;
    char buffer[512];
    while (std::fgets(buffer, sizeof buffer, pipe)) {
        path += buffer;
    }
    pclose(pipe);
    return path;
#endif
}

Font load_font(bool bold) {
    std::vector<std::string> candidates = {matched_font(bold ? "sans-serif:weight=180" : "sans-serif")};
    const char* fallbacks[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",   "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",      "/Library/Fonts/Arial.ttf",
    };
    candidates.insert(candidates.end(), std::begin(fallbacks), std::end(fallbacks));
    if (const char* windows = std::getenv("WINDIR")) {
        std::string folder = std::string(windows) + "/Fonts/";
        candidates.insert(candidates.begin(), folder + (bold ? "seguisb.ttf" : "segoeui.ttf"));
        candidates.push_back(folder + "segoeui.ttf");
        candidates.push_back(folder + "arial.ttf");
    }
    Font font;
    for (const std::string& path : candidates) {
        std::error_code error;
        if (path.empty() || !std::filesystem::is_regular_file(path, error)) {
            continue;
        }
        std::ifstream file(path, std::ios::binary);
        font.data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        int offset = stbtt_GetFontOffsetForIndex(font.data.data(), 0);
        if (offset >= 0 && stbtt_InitFont(&font.info, font.data.data(), offset)) {
            font.ok = true;
            log::write("options", "menu font %s", path.c_str());
            return font;
        }
    }
    log::write("options", "no usable font for the options menu");
    return font;
}

const Font& font(bool bold) {
    static const Font regular = load_font(false);
    static const Font semibold = load_font(true);
    return bold && semibold.ok ? semibold : regular;
}

std::u32string decode(const std::string& text) {
    std::u32string result;
    for (size_t i = 0; i < text.size();) {
        unsigned char lead = static_cast<unsigned char>(text[i]);
        int length = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 1;
        char32_t value = length == 1 ? lead : lead & (0x7F >> length);
        for (int k = 1; k < length && i + k < text.size(); k++) {
            value = (value << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        }
        result.push_back(value);
        i += static_cast<size_t>(length);
    }
    return result;
}

struct Face {
    const Font* font;
    float scale;
    int ascent;
    int descent;
};

Face make_face(bool bold, int height) {
    const Font& chosen = font(bold);
    Face face{&chosen, stbtt_ScaleForMappingEmToPixels(&chosen.info, static_cast<float>(height)), 0, 0};
    int ascent = 0;
    int descent = 0;
    int gap = 0;
    stbtt_GetFontVMetrics(&chosen.info, &ascent, &descent, &gap);
    face.ascent = static_cast<int>(std::lround(ascent * face.scale));
    face.descent = static_cast<int>(std::lround(descent * face.scale));
    return face;
}

float advance(const Face& face, char32_t codepoint, char32_t next) {
    int width = 0;
    int bearing = 0;
    stbtt_GetCodepointHMetrics(&face.font->info, static_cast<int>(codepoint), &width, &bearing);
    int kern = next ? stbtt_GetCodepointKernAdvance(&face.font->info, static_cast<int>(codepoint), static_cast<int>(next)) : 0;
    return (width + kern) * face.scale;
}

float text_width(const Face& face, const std::u32string& text) {
    float width = 0.0f;
    for (size_t i = 0; i < text.size(); i++) {
        width += advance(face, text[i], i + 1 < text.size() ? text[i + 1] : 0);
    }
    return width;
}

struct Canvas {
    int width;
    int height;
    std::vector<uint32_t> pixels;

    void fill(int left, int top, int right, int bottom, uint32_t color) {
        left = std::max(left, 0);
        top = std::max(top, 0);
        right = std::min(right, width);
        bottom = std::min(bottom, height);
        for (int y = top; y < bottom; y++) {
            std::fill(pixels.begin() + static_cast<std::ptrdiff_t>(y) * width + left, pixels.begin() + static_cast<std::ptrdiff_t>(y) * width + right, color);
        }
    }

    void fill_round(int left, int top, int right, int bottom, int radius, uint32_t color) {
        radius = std::max(0, std::min({radius, (right - left) / 2, (bottom - top) / 2}));
        for (int y = std::max(top, 0); y < std::min(bottom, height); y++) {
            for (int x = std::max(left, 0); x < std::min(right, width); x++) {
                double cx = x + 0.5;
                double cy = y + 0.5;
                double ox = cx < left + radius ? left + radius - cx : cx > right - radius ? cx - (right - radius) : 0.0;
                double oy = cy < top + radius ? top + radius - cy : cy > bottom - radius ? cy - (bottom - radius) : 0.0;
                double coverage = 1.0;
                if (ox > 0.0 && oy > 0.0) {
                    coverage = std::clamp(radius - std::sqrt(ox * ox + oy * oy) + 0.5, 0.0, 1.0);
                }
                blend(x, y, color, static_cast<unsigned>(coverage * 255.0 + 0.5));
            }
        }
    }

    void dot(int x, int y, int stroke, uint32_t color) {
        fill(x - stroke / 2, y - stroke / 2, x - stroke / 2 + stroke, y - stroke / 2 + stroke, color);
    }

    void cross(int cx, int cy, int half, int stroke, uint32_t color) {
        for (int k = -half; k <= half; k++) {
            dot(cx + k, cy + k, stroke, color);
            dot(cx + k, cy - k, stroke, color);
        }
    }

    void chevron(int cx, int cy, int half, int direction, int stroke, uint32_t color) {
        for (int k = 0; k <= half; k++) {
            int x = cx + direction * (half / 2 - k);
            dot(x, cy - half + k, stroke, color);
            dot(x, cy + half - k, stroke, color);
        }
    }

    void blend(int x, int y, uint32_t color, unsigned coverage) {
        if (x < 0 || y < 0 || x >= width || y >= height || coverage == 0) {
            return;
        }
        uint32_t& target = pixels[static_cast<size_t>(y) * width + x];
        uint32_t result = 0;
        for (int shift = 0; shift < 24; shift += 8) {
            unsigned below = (target >> shift) & 0xFF;
            unsigned above = (color >> shift) & 0xFF;
            unsigned mixed = (below * (255 - coverage) + above * coverage + 127) / 255;
            result |= mixed << shift;
        }
        target = result;
    }

    void text(const Face& face, uint32_t color, const std::string& value, int left, int top, int right, int bottom, Align align, bool ellipsis) {
        if (!face.font->ok) {
            return;
        }
        std::u32string glyphs = decode(value);
        if (ellipsis && text_width(face, glyphs) > right - left) {
            while (!glyphs.empty() && text_width(face, glyphs + kEllipsis) > right - left) {
                glyphs.pop_back();
            }
            glyphs.push_back(kEllipsis);
        }
        float width = text_width(face, glyphs);
        float x = align == Align::Left ? static_cast<float>(left) : align == Align::Right ? right - width : left + (right - left - width) / 2.0f;
        int baseline = top + (bottom - top - (face.ascent - face.descent)) / 2 + face.ascent;
        for (size_t i = 0; i < glyphs.size(); i++) {
            int codepoint = static_cast<int>(glyphs[i]);
            float origin = std::floor(x);
            float shift = x - origin;
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            stbtt_GetCodepointBitmapBoxSubpixel(&face.font->info, codepoint, face.scale, face.scale, shift, 0.0f, &x0, &y0, &x1, &y1);
            int glyph_width = x1 - x0;
            int glyph_height = y1 - y0;
            if (glyph_width > 0 && glyph_height > 0) {
                std::vector<unsigned char> bitmap(static_cast<size_t>(glyph_width) * glyph_height);
                stbtt_MakeCodepointBitmapSubpixel(&face.font->info, bitmap.data(), glyph_width, glyph_height, glyph_width, face.scale, face.scale, shift, 0.0f,
                                                  codepoint);
                for (int gy = 0; gy < glyph_height; gy++) {
                    for (int gx = 0; gx < glyph_width; gx++) {
                        blend(static_cast<int>(origin) + x0 + gx, baseline + y0 + gy, color, bitmap[static_cast<size_t>(gy) * glyph_width + gx]);
                    }
                }
            }
            x += advance(face, glyphs[i], i + 1 < glyphs.size() ? glyphs[i + 1] : 0);
        }
    }
};

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

}

Image render(const Menu& menu, int client_height) {
    Image image;
    std::vector<Row> rows = menu.rows();
    Layout frame = layout(client_height, rows.size());
    if (frame.width <= 0 || frame.height <= 0 || !font(false).ok) {
        return image;
    }
    Canvas canvas{frame.width, frame.height, std::vector<uint32_t>(static_cast<size_t>(frame.width) * frame.height, kPanel)};
    int unit = frame.unit;
    Face title_face = make_face(true, unit * 3 / 2);
    Face row_face = make_face(false, unit);
    Face small_face = make_face(false, unit * 4 / 5);
    const ui::Text& strings = ui::text();
    const Hover& hover = menu.hover();
    int left = frame.padding;
    int right = frame.width - frame.padding;
    int stroke = std::max(2, unit / 7);
    canvas.text(title_face, kText, strings.menu_title, left, frame.padding, right, frame.padding + frame.title_height - unit / 2, Align::Left, false);
    Rect close = close_rect(frame);
    bool close_hover = hover.part == Part::Close;
    if (close_hover) {
        canvas.fill_round(close.x, close.y, close.x + close.width, close.y + close.height, unit / 2, kDanger);
    }
    canvas.cross(close.x + close.width / 2, close.y + close.height / 2, unit / 3, stroke, close_hover ? kText : kMuted);
    int rows_top = frame.padding + frame.title_height;
    canvas.fill(left, rows_top - unit / 2, right, rows_top - unit / 2 + 1, kSeparator);
    for (size_t i = 0; i < rows.size(); i++) {
        const Row& row = rows[i];
        bool selected = i == menu.selection();
        bool hovered = hover.row == static_cast<int>(i);
        int top = rows_top + static_cast<int>(i) * frame.row_height;
        int bottom = top + frame.row_height;
        if (selected || hovered) {
            canvas.fill_round(left - unit / 2, top + 2, right + unit / 2, bottom - 2, unit / 2, selected ? kSelected : kHover);
        }
        if (selected) {
            canvas.fill_round(left - unit / 2 + unit / 4, top + unit / 2, left - unit / 2 + unit / 4 + std::max(3, unit / 5), bottom - unit / 2, std::max(2, unit / 10), kAccent);
        }
        Rect control = control_rect(frame, i);
        int value_left = control.x;
        if (row.toggle) {
            int width = unit * 3;
            int switch_left = control.x + control.width - width;
            value_left = switch_left;
            uint32_t track = row.on ? kAccent : kOff;
            if (hovered && hover.part != Part::Label) {
                track = row.on ? kAccentHover : kOffHover;
            }
            canvas.fill_round(switch_left, control.y, switch_left + width, control.y + control.height, control.height / 2, track);
            int knob = control.height - unit / 2;
            int knob_left = row.on ? switch_left + width - knob - unit / 4 : switch_left + unit / 4;
            canvas.fill_round(knob_left, control.y + unit / 4, knob_left + knob, control.y + unit / 4 + knob, knob / 2, kText);
        } else {
            canvas.fill_round(control.x, control.y, control.x + control.width, control.y + control.height, control.height / 2, hovered ? kControlHover : kControl);
            int arrow = unit * 2;
            uint32_t previous_color = hovered && hover.part == Part::Previous ? kAccent : kMuted;
            uint32_t next_color = hovered && hover.part == Part::Next ? kAccent : kMuted;
            int cy = control.y + control.height / 2;
            canvas.chevron(control.x + arrow / 2, cy, unit / 3, 1, stroke, previous_color);
            canvas.chevron(control.x + control.width - arrow / 2, cy, unit / 3, -1, stroke, next_color);
            canvas.text(row_face, selected ? kAccent : kText, row.value, control.x + arrow, control.y, control.x + control.width - arrow, control.y + control.height,
                        Align::Center, true);
        }
        int label_right = value_left - unit / 2;
        if (!row.live) {
            int note_width = static_cast<int>(std::ceil(text_width(small_face, decode(strings.menu_restart))));
            int note_left = value_left - unit / 2 - note_width;
            canvas.text(small_face, kMuted, strings.menu_restart, note_left - unit, top, value_left - unit / 2, bottom, Align::Right, false);
            label_right = note_left - unit / 2;
        }
        canvas.text(row_face, kText, row.label, left + unit / 2, top, std::max(left + unit, label_right), bottom, Align::Left, true);
    }
    int footer_top = rows_top + static_cast<int>(rows.size()) * frame.row_height;
    canvas.fill(left, footer_top + unit / 2, right, footer_top + unit / 2 + 1, kSeparator);
    canvas.text(small_face, kMuted, strings.menu_hint, left, footer_top + unit / 2, right, footer_top + frame.footer_height, Align::Center, true);
    image.width = static_cast<uint32_t>(frame.width);
    image.height = static_cast<uint32_t>(frame.height);
    image.pixels.resize(canvas.pixels.size());
    int radius = unit * 3 / 4;
    for (int y = 0; y < frame.height; y++) {
        for (int x = 0; x < frame.width; x++) {
            size_t index = static_cast<size_t>(y) * image.width + static_cast<size_t>(x);
            image.pixels[index] = (canvas.pixels[index] & 0xFFFFFF) | (corner_alpha(x, y, frame.width, frame.height, radius) << 24);
        }
    }
    return image;
}

Image render_titlebar(int width, int height, const std::string& title, int hover, int pressed, bool maximized, bool focused) {
    Image image;
    if (width <= 0 || height <= 0) {
        return image;
    }
    constexpr uint32_t kBar = 0x161618;
    constexpr uint32_t kCloseHover = 0xE81123;
    constexpr uint32_t kClosePressed = 0xB50E1C;
    constexpr int kButtonCount = 3;
    Canvas canvas{width, height, std::vector<uint32_t>(static_cast<size_t>(width) * height, kBar)};
    int button = height * 46 / 32;
    int stroke = std::max(1, height / 32);
    int half = std::max(4, height * 5 / 32);
    int step = std::max(2, height * 2 / 32);
    int buttons_left = width - button * kButtonCount;
    int margin = height * 12 / 32;
    if (font(false).ok) {
        Face face = make_face(false, std::max(8, height * 13 / 32));
        canvas.text(face, focused ? kText : kMuted, title, margin, 0, std::max(margin, buttons_left - margin), height - stroke, Align::Left, true);
    }
    canvas.fill(0, height - stroke, width, height, kSeparator);
    for (int i = 0; i < kButtonCount; i++) {
        int left = buttons_left + i * button;
        bool close = i == kButtonCount - 1;
        bool over = i == hover;
        if (over) {
            canvas.fill(left, 0, left + button, height - stroke, close ? (pressed == i ? kClosePressed : kCloseHover) : (pressed == i ? kSeparator : kSelected));
        }
        uint32_t color = over && close ? kText : focused ? kText : kMuted;
        int cx = left + button / 2;
        int cy = (height - stroke) / 2;
        if (i == 0) {
            canvas.fill(cx - half, cy, cx + half + 1, cy + stroke, color);
        } else if (i == 1 && !maximized) {
            canvas.fill(cx - half, cy - half, cx + half + 1, cy - half + stroke, color);
            canvas.fill(cx - half, cy + half, cx + half + 1, cy + half + stroke, color);
            canvas.fill(cx - half, cy - half, cx - half + stroke, cy + half + stroke, color);
            canvas.fill(cx + half, cy - half, cx + half + stroke, cy + half + stroke, color);
        } else if (i == 1) {
            canvas.fill(cx - half, cy - half + step, cx + half - step + 1, cy - half + step + stroke, color);
            canvas.fill(cx - half, cy + half, cx + half - step + 1, cy + half + stroke, color);
            canvas.fill(cx - half, cy - half + step, cx - half + stroke, cy + half + stroke, color);
            canvas.fill(cx + half - step, cy - half + step, cx + half - step + stroke, cy + half + stroke, color);
            canvas.fill(cx - half + step, cy - half, cx + half + 1, cy - half + stroke, color);
            canvas.fill(cx + half, cy - half, cx + half + stroke, cy + half - step + stroke, color);
        } else {
            for (int k = -half; k <= half; k++) {
                canvas.fill(cx + k, cy + k, cx + k + stroke, cy + k + stroke, color);
                canvas.fill(cx + k, cy - k, cx + k + stroke, cy - k + stroke, color);
            }
        }
    }
    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);
    image.pixels.resize(canvas.pixels.size());
    for (size_t i = 0; i < canvas.pixels.size(); i++) {
        image.pixels[i] = (canvas.pixels[i] & 0xFFFFFF) | 0xFF000000u;
    }
    return image;
}

}
