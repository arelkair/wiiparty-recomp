#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "wp/cpu.h"
#include "wp/function_table.h"
#include "wp/input.h"
#include "wp/ios.h"
#include "wp/keymap.h"
#include "wp/modules.h"
#include "wp/screenshot.h"
#include "wp/settings.h"
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

}

void test_key_names() {
    using namespace wp::keymap;
    CHECK(key_code("A") == 'A');
    CHECK(key_code("a") == 'A');
    CHECK(key_code("7") == '7');
    CHECK(key_code("F1") == 0x70);
    CHECK(key_code("f24") == 0x87);
    CHECK(key_code("F25") == -1);
    CHECK(key_code("F0") == -1);
    CHECK(key_code("F01") == -1);
    CHECK(key_code("Enter") == 0x0D);
    CHECK(key_code(" space ") == 0x20);
    CHECK(key_code("BACKSPACE") == 0x08);
    CHECK(key_code("Tab") == 0x09);
    CHECK(key_code("Shift") == 0x10);
    CHECK(key_code("LeftShift") == 0xA0);
    CHECK(key_code("Ctrl") == 0x11);
    CHECK(key_code("Alt") == 0x12);
    CHECK(key_code("Up") == 0x26);
    CHECK(key_code("Down") == 0x28);
    CHECK(key_code("Left") == 0x25);
    CHECK(key_code("Right") == 0x27);
    CHECK(key_code("Plus") == 0xBB);
    CHECK(key_code("Minus") == 0xBD);
    CHECK(key_code("NumPlus") == 0x6B);
    CHECK(key_code("MouseLeft") == 0x01);
    CHECK(key_code("MouseRight") == 0x02);
    CHECK(key_code("MouseMiddle") == 0x04);
    CHECK(key_code("MouseX1") == 0x05);
    CHECK(key_code("MouseX2") == 0x06);
    CHECK(key_code("WheelUp") == kWheelUp);
    CHECK(key_code("WheelDown") == kWheelDown);
    CHECK(key_code("") == -1);
    CHECK(key_code("Banana") == -1);
    CHECK(key_code("AB") == -1);
    CHECK(key_name('Q') == "Q");
    CHECK(key_name(0x7A) == "F11");
    CHECK(key_name(0xA0) == "LeftShift");
    CHECK(key_name(kWheelDown) == "WheelDown");
    CHECK(key_name(0x3000).empty());
    for (int code = 0; code < 0x200; code++) {
        std::string name = key_name(code);
        CHECK(name.empty() || key_code(name) == code);
    }
    CHECK(reserved(key_code("F11")));
    CHECK(reserved(key_code("F12")));
    CHECK(!reserved(key_code("F10")));
    Parsed parsed = parse(" Enter , space,MouseLeft,,Nope,F12,enter,F11 ");
    CHECK(parsed.codes.size() == 3);
    CHECK(format(parsed.codes) == "Enter,Space,MouseLeft");
    CHECK(format(parsed.codes, ", ") == "Enter, Space, MouseLeft");
    CHECK(parsed.invalid.size() == 3);
    CHECK(parsed.invalid.size() == 3 && parsed.invalid[0] == "Nope" && parsed.invalid[1] == "F12" && parsed.invalid[2] == "F11");
    CHECK(parse("").codes.empty() && parse("").invalid.empty());
}

void test_key_defaults() {
    using namespace wp::keymap;
    Bindings keys = defaults();
    CHECK(format(keys.of(Action::A)) == "Enter,Space,MouseLeft");
    CHECK(format(keys.of(Action::B)) == "Backspace,MouseRight");
    CHECK(format(keys.of(Action::Plus)) == "Plus,NumPlus");
    CHECK(format(keys.of(Action::Minus)) == "Minus,NumMinus");
    CHECK(format(keys.of(Action::Home)) == "H");
    CHECK(format(keys.of(Action::Up)) == "Up,A");
    CHECK(format(keys.of(Action::Right)) == "Right,W");
    CHECK(format(keys.of(Action::Shake)) == "MouseMiddle,LeftShift");
    CHECK(format(keys.of(Action::SwingUp)) == "WheelUp,T");
    CHECK(format(keys.of(Action::SwingDown)) == "WheelDown,G");
    CHECK(format(keys.of(Action::Grip)) == "Tab");
    CHECK(format(keys.of(Action::Screenshot)) == "F10");
    CHECK(action(Action::Screenshot).buttons == 0);
    CHECK(setting_key(static_cast<size_t>(Action::Screenshot)) == "keys.screenshot");
    CHECK(action(Action::A).buttons == wp::input::kButtonA);
    CHECK(action(Action::TiltDown).buttons == wp::input::kMotionTiltDown);
    CHECK(action(Action::Grip).buttons == 0);
    CHECK(keys.has(Action::A, 0x0D));
    CHECK(!keys.has(Action::B, 0x0D));
    CHECK(keys.bound('Q'));
    CHECK(!keys.bound(0x7A));
    CHECK(!keys.bound(0x7B));
    for (size_t i = 0; i < kActionCount; i++) {
        CHECK(parse(action(i).defaults).invalid.empty());
        CHECK(!keys.codes[i].empty());
        CHECK(setting_key(i) == std::string("keys.") + action(i).name);
    }
    std::map<std::string, std::string> file = {{"keys.a", "K,Wrong"}, {"keys.grip", ""}, {"keys.b", "F12"}};
    std::vector<std::string> invalid;
    Bindings loaded = load(
        [&](const std::string& key) {
            auto it = file.find(key);
            if (it != file.end()) {
                return it->second;
            }
            for (size_t i = 0; i < kActionCount; i++) {
                if (setting_key(i) == key) {
                    return std::string(action(i).defaults);
                }
            }
            return std::string();
        },
        invalid);
    CHECK(format(loaded.of(Action::A)) == "K");
    CHECK(loaded.of(Action::Grip).empty());
    CHECK(loaded.of(Action::B).empty());
    CHECK(format(loaded.of(Action::Home)) == "H");
    CHECK(invalid.size() == 2 && invalid[0] == "keys.a: Wrong" && invalid[1] == "keys.b: F12");
}

void test_settings_file_keys() {
    const char* path = "runtime_tests_settings.ini";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "[keys]\na = K, MouseX1\nunknown=1\n";
    }
    wp::settings::load(path);
    CHECK(wp::settings::text("keys.a", nullptr) == "K, MouseX1");
    CHECK(wp::settings::text("keys.grip", nullptr) == "Tab");
    CHECK(wp::settings::text("video.scale", nullptr) == "1");
    std::ifstream file(path, std::ios::binary);
    std::stringstream contents;
    contents << file.rdbuf();
    std::string text = contents.str();
    CHECK(text.find("[keys]\n") != std::string::npos);
    CHECK(text.find("a=K, MouseX1\n") != std::string::npos);
    CHECK(text.find("unknown") == std::string::npos);
    CHECK(text.find("[video]\nscale=1\n") == 0);
    for (size_t i = 0; i < wp::keymap::kActionCount; i++) {
        std::string line = "\n" + std::string(wp::keymap::action(i).name) + "=";
        CHECK(text.find(line, text.find("[keys]")) != std::string::npos);
    }
    file.close();
    std::remove(path);
}

void test_screenshot_names() {
    std::tm time{};
    time.tm_year = 2026 - 1900;
    time.tm_mon = 8;
    time.tm_mday = 6;
    time.tm_hour = 9;
    time.tm_min = 5;
    time.tm_sec = 3;
    CHECK(wp::screenshot::file_name("wiiparty", time, 1) == "wiiparty_2026-09-06_09-05-03.png");
    CHECK(wp::screenshot::file_name("wiiparty", time, 2) == "wiiparty_2026-09-06_09-05-03_2.png");
    CHECK(wp::screenshot::file_name("wiiparty", time, 13) == "wiiparty_2026-09-06_09-05-03_13.png");
    time.tm_mon = 11;
    time.tm_mday = 31;
    time.tm_hour = 23;
    time.tm_min = 59;
    time.tm_sec = 59;
    CHECK(wp::screenshot::file_name("game", time, 1) == "game_2026-12-31_23-59-59.png");
    CHECK(std::string(wp::screenshot::kFolder) == "screenshots");
    std::vector<uint8_t> png = wp::video::encode_png({0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFFFF, 0, 0x00123456}, 3, 2);
    const uint8_t signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    CHECK(png.size() > 45 && std::equal(signature, signature + 8, png.begin()));
    CHECK(png.size() > 24 && png[12] == 'I' && png[13] == 'H' && png[14] == 'D' && png[15] == 'R' && png[19] == 3 && png[23] == 2);
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
    test_key_names();
    test_key_defaults();
    test_settings_file_keys();
    test_screenshot_names();
    std::free(wp::g_memory);
    if (failures == 0) {
        std::puts("all runtime tests passed");
    }
    return failures == 0 ? 0 : 1;
}
