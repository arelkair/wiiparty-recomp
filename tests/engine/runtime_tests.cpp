#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "wp/cpu.h"
#include "wp/function_table.h"
#include "wp/ios.h"
#include "wp/modules.h"
#include "wp/options.h"
#include "wp/options_window.h"
#include "wp/settings.h"
#include "wp/ui_text.h"
#include "wp/video.h"

namespace wp {
const FunctionEntry g_function_table[1] = {};
const size_t g_function_count = 0;
const FunctionEntry g_resume_table[1] = {};
const size_t g_resume_count = 0;
const ModuleDescriptor* const g_module_table[1] = {nullptr};
const size_t g_module_count = 0;
const NameEntry g_name_table[1] = {};
const size_t g_name_count = 0;
}

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        std::fprintf(stderr, "line %d: %s\n", line, expression);
        failures++;
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

void test_memory() {
    wp::wr32(0x80001000, 0x11223344);
    CHECK(wp::rd8(0x80001000) == 0x11);
    CHECK(wp::rd16(0x80001002) == 0x3344);
    CHECK(wp::rd32(0x80001000) == 0x11223344);
    CHECK(wp::rd32_reversed(0x80001000) == 0x44332211);
    wp::wr64(0x80001008, 0x0102030405060708ull);
    CHECK(wp::rd32(0x80001008) == 0x01020304);
    CHECK(wp::rd64(0x80001008) == 0x0102030405060708ull);
    CHECK(wp::host(0x80001000) == wp::host(0xC0001000));
    wp::wrf32(0x80001010, 1.5f);
    CHECK(wp::rd32(0x80001010) == 0x3FC00000);
    CHECK(wp::rdf32(0x80001010) == 1.5f);
    wp::wrf64(0x80001018, -2.25);
    CHECK(wp::rdf64(0x80001018) == -2.25);
    wp::wr32(0x80002000, 0xFFFFFFFF);
    wp::wr32(0x80002020, 0xFFFFFFFF);
    wp::dcbz(0x80002010);
    CHECK(wp::rd32(0x80002000) == 0);
    CHECK(wp::rd32(0x8000201C) == 0);
    CHECK(wp::rd32(0x80002020) == 0xFFFFFFFF);
}

void test_rotate_and_shift() {
    wp::Cpu c{};
    CHECK(wp::rotl(0x80000001u, 1) == 3);
    CHECK(wp::rotl(0x12345678u, 0) == 0x12345678);
    CHECK(wp::rotl(0x12345678u, 32) == 0x12345678);
    CHECK(wp::slw(1, 31) == 0x80000000);
    CHECK(wp::slw(1, 32) == 0);
    CHECK(wp::srw(0x80000000u, 31) == 1);
    CHECK(wp::srw(0x80000000u, 32) == 0);
    CHECK(wp::sraw(c, 0xFFFFFFF0u, 4) == 0xFFFFFFFF);
    CHECK(c.xer_ca == 0);
    CHECK(wp::sraw(c, 0xFFFFFFF1u, 4) == 0xFFFFFFFF);
    CHECK(c.xer_ca == 1);
    CHECK(wp::sraw(c, 0x80000000u, 40) == 0xFFFFFFFF);
    CHECK(c.xer_ca == 1);
    CHECK(wp::sraw(c, 0x40000000u, 40) == 0);
    CHECK(c.xer_ca == 0);
}

void test_arithmetic() {
    wp::Cpu c{};
    CHECK(wp::add_carry(c, 0xFFFFFFFFu, 1, 0) == 0);
    CHECK(c.xer_ca == 1);
    CHECK(wp::add_carry(c, 1, 2, 0) == 3);
    CHECK(c.xer_ca == 0);
    c.xer_ca = 1;
    CHECK(wp::add_extended(c, 1, 2) == 4);
    c.xer_ca = 0;
    CHECK(wp::sub_carry(c, 3, 5) == 2);
    CHECK(c.xer_ca == 1);
    CHECK(wp::sub_carry(c, 5, 3) == 0xFFFFFFFE);
    CHECK(c.xer_ca == 0);
    CHECK(wp::sub_carry(c, 5, 5) == 0);
    CHECK(c.xer_ca == 1);
    CHECK(wp::mulhw(0xFFFFFFFFu, 2) == 0xFFFFFFFF);
    CHECK(wp::mulhwu(0xFFFFFFFFu, 2) == 1);
    CHECK(wp::divw(static_cast<uint32_t>(-7), 2) == static_cast<uint32_t>(-3));
    CHECK(wp::divw(5, 0) == 0);
    CHECK(wp::divwu(7, 2) == 3);
    CHECK(wp::cntlzw(0) == 32);
    CHECK(wp::cntlzw(1) == 31);
    CHECK(wp::cntlzw(0x80000000u) == 0);
    CHECK(wp::sign_extend8(0x80) == 0xFFFFFF80);
    CHECK(wp::sign_extend16(0x7FFF) == 0x7FFF);
}

void test_condition_register() {
    wp::Cpu c{};
    wp::compare_signed(c, 3, 0xFFFFFFFFu, 1);
    CHECK(c.cr[3] == 8);
    wp::compare_unsigned(c, 3, 0xFFFFFFFFu, 1);
    CHECK(c.cr[3] == 4);
    wp::compare_signed(c, 3, 5, 5);
    CHECK(c.cr[3] == 2);
    CHECK(wp::cr_get(c, 14));
    CHECK(!wp::cr_get(c, 13));
    CHECK(!wp::cr_get(c, 12));
    wp::cr_set(c, 12, true);
    CHECK(c.cr[3] == 10);
    wp::mtcrf(c, 0xFF, 0x12345678);
    CHECK(wp::mfcr(c) == 0x12345678);
    wp::mtcrf(c, 0x80, 0xF0000000);
    CHECK(wp::mfcr(c) == 0xF2345678);
    c.xer_so = 1;
    wp::record(c, 0);
    CHECK(c.cr[0] == 3);
    wp::mtxer(c, 0xE0000005);
    CHECK(c.xer_so == 1 && c.xer_ov == 1 && c.xer_ca == 1 && c.xer_byte_count == 5);
    CHECK(wp::mfxer(c) == 0xE0000005);
}

void test_floating_point() {
    wp::Cpu c{};
    wp::compare_float(c, 1, 1.0, 2.0);
    CHECK(c.cr[1] == 8);
    wp::compare_float(c, 1, 2.0, 1.0);
    CHECK(c.cr[1] == 4);
    wp::compare_float(c, 1, 2.0, 2.0);
    CHECK(c.cr[1] == 2);
    wp::compare_float(c, 1, std::nan(""), 2.0);
    CHECK(c.cr[1] == 1);
    CHECK(wp::fpr_bits(wp::convert_to_int(-1.9, true)) == 0xFFF80000FFFFFFFFull);
    CHECK(wp::fpr_bits(wp::convert_to_int(2.5, false)) == 0xFFF8000000000002ull);
    CHECK(wp::fpr_bits(wp::convert_to_int(3e10, true)) == 0xFFF800007FFFFFFFull);
    CHECK(wp::fsel(-1.0, 10.0, 20.0) == 10.0);
    CHECK(wp::fsel(0.0, 10.0, 20.0) == 20.0);
    CHECK(wp::round_single(0.1) == static_cast<double>(0.1f));
}

void test_paired_singles() {
    wp::Cpu c{};
    c.f[1] = 1.0;
    c.ps1[1] = 2.0;
    c.f[2] = 10.0;
    c.ps1[2] = 20.0;
    wp::ps_add(c, 3, 1, 2);
    CHECK(c.f[3] == 11.0 && c.ps1[3] == 22.0);
    wp::ps_merge(c, 4, 1, 2, true, false);
    CHECK(c.f[4] == 2.0 && c.ps1[4] == 10.0);
    wp::ps_madd(c, 5, 1, 2, 1, 1.0, 1.0);
    CHECK(c.f[5] == 11.0 && c.ps1[5] == 42.0);
    wp::ps_madd(c, 5, 1, 2, 1, -1.0, 1.0);
    CHECK(c.f[5] == -9.0 && c.ps1[5] == -38.0);
    wp::ps_sum0(c, 6, 1, 2, 1);
    CHECK(c.f[6] == 3.0 && c.ps1[6] == 20.0);
    wp::ps_muls1(c, 7, 1, 2);
    CHECK(c.f[7] == 20.0 && c.ps1[7] == 40.0);
}

void test_quantization() {
    wp::Cpu c{};
    uint32_t size = 0;
    c.spr[wp::kSprGqr0] = 0;
    wp::wrf32(0x80003000, 3.5f);
    wp::wrf32(0x80003004, -1.25f);
    wp::load_quantized(c, 1, 0x80003000, 0, 0, size);
    CHECK(c.f[1] == 3.5 && c.ps1[1] == -1.25 && size == 8);
    wp::load_quantized(c, 2, 0x80003000, 1, 0, size);
    CHECK(c.f[2] == 3.5 && c.ps1[2] == 1.0 && size == 4);

    c.spr[wp::kSprGqr0 + 1] = (7u << 16) | (4u << 24);
    wp::wr16(0x80003010, 0xFFF0);
    wp::wr16(0x80003012, 32);
    wp::load_quantized(c, 3, 0x80003010, 0, 1, size);
    CHECK(c.f[3] == -1.0 && c.ps1[3] == 2.0 && size == 4);

    c.spr[wp::kSprGqr0 + 2] = 7u | (4u << 8);
    c.f[4] = -1.0;
    c.ps1[4] = 2.0;
    wp::store_quantized(c, 4, 0x80003020, 0, 2, size);
    CHECK(wp::rd16(0x80003020) == 0xFFF0 && wp::rd16(0x80003022) == 32 && size == 4);

    c.spr[wp::kSprGqr0 + 3] = 4u | (0u << 8);
    c.f[5] = 300.0;
    wp::store_quantized(c, 5, 0x80003030, 1, 3, size);
    CHECK(wp::rd8(0x80003030) == 255 && size == 1);
}

void test_disc_drive() {
    int32_t drive = wp::ios::open("/dev/di");
    const uint32_t command = 0x80200000;
    const uint32_t output = 0x80201000;

    wp::wr32(command + 4, 0x20);
    wp::wr32(command + 8, 0x460A0000);
    CHECK(wp::ios::ioctl(drive, 0x8D, command, 0x20, output, 0x20) == 2);
    CHECK(wp::ios::ioctl(drive, 0xE0, command, 0x20, output, 0x20) == 1);
    CHECK(wp::rd32(output) == 0x52100);
    CHECK(wp::ios::ioctl(drive, 0xE0, command, 0x20, output, 0x20) == 1);
    CHECK(wp::rd32(output) == 0);

    wp::wr32(command + 8, 0);
    CHECK(wp::ios::ioctl(drive, 0x8D, command, 0x20, output, 0x20) == 1);

    CHECK(wp::ios::ioctl(drive, 0xA4, command, 0x20, output, 0x20) == 2);
    CHECK(wp::ios::ioctl(drive, 0xE0, command, 0x20, output, 0x20) == 1);
    CHECK(wp::rd32(output) == 0x53100);
}

void test_live_settings() {
    const char* path = "runtime_tests_settings.ini";
    wp::settings::load(path);
    wp::settings::LiveFlag mute("audio.mute", nullptr);
    wp::settings::LiveNumber scale("video.scale", nullptr);
    CHECK(!mute());
    CHECK(scale() == 1);
    uint32_t revision = wp::settings::revision();
    wp::settings::store("audio.mute", "1");
    wp::settings::store("video.scale", "3");
    CHECK(wp::settings::revision() == revision + 2);
    CHECK(mute());
    CHECK(scale() == 3);
    wp::settings::load(path);
    CHECK(mute());
    CHECK(scale() == 3);
    std::vector<std::string> keys = wp::settings::keys();
    CHECK(std::find(keys.begin(), keys.end(), "system.options_menu") != keys.end());
    CHECK(wp::settings::flag("system.options_menu", nullptr));
    wp::settings::load("");
    CHECK(!mute());
    CHECK(scale() == 1);
    std::remove(path);
}

void test_options_menu() {
    wp::settings::load("");
    std::vector<std::string> keys = wp::settings::keys();
    for (wp::ui::Language language : {wp::ui::Language::English, wp::ui::Language::Spanish}) {
        wp::ui::set_language(language);
        for (const std::string& key : keys) {
            CHECK(key != wp::ui::label(key.c_str()));
            CHECK(!wp::options::choices(key).empty());
        }
    }
    wp::ui::set_language(wp::ui::Language::English);
    CHECK(wp::options::live("video.scale"));
    CHECK(wp::options::live("video.fullscreen"));
    CHECK(wp::options::live("audio.mute"));
    CHECK(!wp::options::live("system.language"));
    CHECK(!wp::options::live("system.pal60"));
    CHECK(wp::options::step_value("audio.mute", "0", 1) == "1");
    CHECK(wp::options::step_value("audio.mute", "1", 1) == "0");
    CHECK(wp::options::step_value("audio.mute", "off", -1) == "1");
    CHECK(wp::options::step_value("audio.mute", "yes", 1) == "0");
    CHECK(wp::options::step_value("video.scale", "6", 1) == "1");
    CHECK(wp::options::step_value("video.scale", "1", -1) == "6");
    CHECK(wp::options::step_value("video.scale", "9", 1) == "1");
    CHECK(wp::options::step_value("system.language", "auto", 1) == "en");
    CHECK(wp::options::step_value("system.language", "auto", -1) == "nl");
    CHECK(wp::options::step_value("unknown.key", "x", 1) == "x");
    CHECK(wp::options::shown_value("video.scale", "1") == "Native");
    CHECK(wp::options::shown_value("video.scale", "4") == "4x");
    CHECK(wp::options::shown_value("system.language", "auto") == "System");
    CHECK(wp::options::shown_value("system.language", "de") == "Deutsch");
    CHECK(wp::options::shown_value("audio.mute", "1") == "On");
    wp::ui::set_language(wp::ui::Language::Spanish);
    CHECK(wp::options::shown_value("audio.mute", "0") == "No");
    CHECK(std::strcmp(wp::ui::label("audio.mute"), "Silenciar") == 0);
    wp::ui::set_language(wp::ui::Language::English);

    wp::options::Menu menu;
    CHECK(!menu.open());
    menu.set_open(true);
    std::vector<wp::options::Row> rows = menu.rows();
    CHECK(rows.size() == keys.size());
    CHECK(rows[0].key == "video.scale" && rows[0].value == "Native" && rows[0].live);
    CHECK(menu.selection() == 0);
    CHECK(menu.handle(wp::options::Action::Up).empty());
    CHECK(menu.selection() == keys.size() - 1);
    CHECK(menu.handle(wp::options::Action::Down).empty());
    CHECK(menu.selection() == 0);
    CHECK(menu.handle(wp::options::Action::Next) == "video.scale");
    CHECK(wp::settings::number("video.scale", nullptr) == 2);
    CHECK(menu.rows()[0].value == "2x");
    CHECK(menu.handle(wp::options::Action::Previous) == "video.scale");
    CHECK(menu.handle(wp::options::Action::Previous) == "video.scale");
    CHECK(wp::settings::number("video.scale", nullptr) == 6);
    size_t mute = static_cast<size_t>(std::find(keys.begin(), keys.end(), "audio.mute") - keys.begin());
    menu.select(mute);
    CHECK(menu.selection() == mute);
    menu.select(keys.size());
    CHECK(menu.selection() == mute);
    wp::settings::LiveFlag muted("audio.mute", nullptr);
    CHECK(!muted());
    CHECK(menu.handle(wp::options::Action::Next) == "audio.mute");
    CHECK(muted());
    CHECK(menu.handle(wp::options::Action::Close).empty());
    CHECK(!menu.open());
    wp::settings::load("");
}

void test_options_repeat_and_layout() {
    wp::options::Repeat repeat(0x3, 400, 100);
    CHECK(repeat.update(0x1, 0) == 0x1);
    CHECK(repeat.update(0x1, 100) == 0);
    CHECK(repeat.update(0x1, 399) == 0);
    CHECK(repeat.update(0x1, 400) == 0x1);
    CHECK(repeat.update(0x1, 450) == 0);
    CHECK(repeat.update(0x1, 500) == 0x1);
    CHECK(repeat.update(0x5, 510) == 0x4);
    CHECK(repeat.update(0x5, 700) == 0x1);
    CHECK(repeat.update(0x0, 800) == 0);
    CHECK(repeat.update(0x4, 810) == 0x4);
    CHECK(repeat.update(0x4, 5000) == 0);

    wp::options::Layout frame = wp::options::layout(720, 12);
    CHECK(frame.unit == 18);
    CHECK(frame.height == frame.padding * 2 + frame.title_height + 12 * frame.row_height + frame.footer_height);
    CHECK(frame.height <= 720);
    CHECK(wp::options::layout(100, 12).unit == 14);
    CHECK(wp::options::layout(4000, 12).unit == 40);
    wp::options::Rect placed = wp::options::place(1280, 720, frame.width, frame.height);
    CHECK(placed.width == frame.width && placed.height == frame.height);
    CHECK(placed.x == (1280 - frame.width) / 2 && placed.y == (720 - frame.height) / 2);
    wp::options::Rect small = wp::options::place(frame.width / 2, 720, frame.width, frame.height);
    CHECK(small.width == frame.width / 2 && small.x == 0);
    int top = placed.y + frame.padding + frame.title_height;
    int middle = placed.x + frame.width / 2;
    CHECK(wp::options::row_at(frame, placed, middle, top) == 0);
    CHECK(wp::options::row_at(frame, placed, middle, top + frame.row_height * 3 + 1) == 3);
    CHECK(wp::options::row_at(frame, placed, middle, top - 1) == -1);
    CHECK(wp::options::row_at(frame, placed, middle, top + frame.row_height * 12) == -1);
    CHECK(wp::options::row_at(frame, placed, placed.x + 1, top) == -1);
    CHECK(wp::options::row_at(frame, placed, placed.x - 100, top) == -1);

    uint64_t version = 0;
    wp::options::Image image;
    bool visible = false;
    CHECK(!wp::options::take_overlay(version, image, visible));
    wp::options::show_overlay(wp::options::Image{{1, 2, 3, 4}, 2, 2});
    CHECK(wp::options::take_overlay(version, image, visible));
    CHECK(visible && image.width == 2 && image.pixels.size() == 4);
    CHECK(!wp::options::take_overlay(version, image, visible));
    wp::options::hide_overlay();
    CHECK(wp::options::take_overlay(version, image, visible));
    CHECK(!visible && image.pixels.empty());
}

void save_menu_png(const wp::options::Image& image, const std::string& path) {
    std::vector<uint32_t> composed(image.pixels.size());
    for (uint32_t y = 0; y < image.height; y++) {
        for (uint32_t x = 0; x < image.width; x++) {
            size_t index = static_cast<size_t>(y) * image.width + x;
            uint32_t pixel = image.pixels[index];
            uint32_t alpha = pixel >> 24;
            uint32_t back = ((x / 16 + y / 16) % 2) ? 0x3CA0E0 : 0xF0C040;
            uint32_t result = 0;
            for (int shift = 0; shift <= 16; shift += 8) {
                uint32_t front = (pixel >> shift) & 0xFF;
                uint32_t behind = (back >> shift) & 0xFF;
                result |= ((front * alpha + behind * (255 - alpha)) / 255) << shift;
            }
            composed[index] = result;
        }
    }
    wp::video::save_png(path.c_str(), composed, image.width, image.height);
}

void test_options_render() {
    wp::settings::load("");
    wp::options::Menu menu;
    menu.set_open(true);
    menu.select(1);
    const char* prefix = std::getenv("WP_MENU_PNG");
    for (int height : {480, 720, 1080}) {
        for (wp::ui::Language language : {wp::ui::Language::English, wp::ui::Language::Spanish}) {
            wp::ui::set_language(language);
            wp::options::Image image = wp::options::render(menu, height);
            wp::options::Layout frame = wp::options::layout(height, wp::settings::keys().size());
            CHECK(image.width == static_cast<uint32_t>(frame.width) && image.height == static_cast<uint32_t>(frame.height));
            CHECK(image.pixels.size() == static_cast<size_t>(image.width) * image.height);
            if (image.pixels.empty()) {
                continue;
            }
            CHECK((image.pixels[0] >> 24) == 0);
            CHECK((image.pixels[image.pixels.size() / 2] >> 24) > 200);
            size_t accent = 0;
            size_t white = 0;
            for (uint32_t pixel : image.pixels) {
                accent += (pixel & 0xFFFFFF) == 0xD4146F;
                white += (pixel & 0xFFFFFF) == 0xFFFFFF;
            }
            CHECK(accent > 50);
            CHECK(white > 100);
            if (prefix) {
                save_menu_png(image, std::string(prefix) + "_" + std::to_string(height) + (language == wp::ui::Language::Spanish ? "_es.png" : "_en.png"));
            }
        }
    }
    wp::ui::set_language(wp::ui::Language::English);
}

}

int main() {
    wp::g_memory = static_cast<uint8_t*>(std::calloc(wp::kMemorySize, 1));
    test_memory();
    test_rotate_and_shift();
    test_arithmetic();
    test_condition_register();
    test_floating_point();
    test_paired_singles();
    test_quantization();
    test_disc_drive();
    test_live_settings();
    test_options_menu();
    test_options_repeat_and_layout();
    test_options_render();
    std::free(wp::g_memory);
    if (failures == 0) {
        std::puts("all runtime tests passed");
    }
    return failures == 0 ? 0 : 1;
}
