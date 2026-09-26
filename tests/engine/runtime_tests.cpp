#include <cstdio>
#include <cstdlib>
#include <string>

#include "wp/cpu.h"
#include "wp/function_table.h"
#include "wp/gx_state.h"
#include "wp/ios.h"
#include "wp/modules.h"

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

int blend_factor(wp::gx::BlendFactor factor, int written, int tev, int destination) {
    using wp::gx::BlendFactor;
    switch (factor) {
    case BlendFactor::Zero:
        return 0;
    case BlendFactor::One:
        return 255;
    case BlendFactor::SourceColor:
    case BlendFactor::SourceAlpha:
        return written;
    case BlendFactor::InverseSourceColor:
    case BlendFactor::InverseSourceAlpha:
        return 255 - written;
    case BlendFactor::DestinationColor:
    case BlendFactor::DestinationAlpha:
        return destination;
    case BlendFactor::InverseDestinationColor:
    case BlendFactor::InverseDestinationAlpha:
        return 255 - destination;
    case BlendFactor::TevColor:
    case BlendFactor::TevAlpha:
        return tev;
    case BlendFactor::InverseTevAlpha:
        return 255 - tev;
    }
    return -1;
}

int blend_channel(const wp::gx::BlendState& state, bool alpha, int tev, int destination) {
    using wp::gx::LogicSource;
    int written = state.output == LogicSource::Tev ? tev : (state.output == LogicSource::Zero ? 0 : (state.output == LogicSource::One ? 255 : 255 - tev));
    if (!state.enable) {
        return written;
    }
    double source = written * blend_factor(alpha ? state.source_alpha : state.source, written, tev, destination) / (255.0 * 255.0);
    double target = destination * blend_factor(alpha ? state.destination_alpha : state.destination, written, tev, destination) / (255.0 * 255.0);
    wp::gx::BlendOperation operation = alpha ? state.alpha_operation : state.operation;
    double value = operation == wp::gx::BlendOperation::Add ? source + target
                   : (operation == wp::gx::BlendOperation::Subtract ? source - target : target - source);
    value = value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
    return static_cast<int>(value * 255.0 + 0.5);
}

int logic_reference(uint32_t op, int s, int d) {
    int results[16] = {0, s & d, s & ~d, s, ~s & d, d, s ^ d, s | d, ~(s | d), ~(s ^ d), ~d, s | ~d, ~s, ~s | d, ~(s & d), 255};
    return results[op] & 255;
}

void test_gx_logic_ops() {
    using namespace wp::gx;
    for (uint32_t op = 0; op < 16; op++) {
        uint32_t mode = 2 | (1u << 3) | (1u << 4) | (op << 12);
        BlendState state = blend_state(mode, true, false, true);
        CHECK(state.logic_op);
        CHECK(state.logic_mode == op);
        if (op == 5) {
            CHECK(!state.color_update && !state.alpha_update && !state.enable);
            CHECK(blend_state(mode, true, true, true).alpha_update);
            continue;
        }
        CHECK(state.color_update && state.alpha_update);
        bool binary = true;
        for (int s = 0; s <= 255; s += 255) {
            for (int d = 0; d <= 255; d += 255) {
                binary &= blend_channel(state, false, s, d) == logic_reference(op, s, d);
                binary &= blend_channel(state, true, s, d) == logic_reference(op, s, d);
            }
        }
        CHECK(binary);
        bool exact = true;
        for (int s = 0; s < 256; s++) {
            for (int d = 0; d < 256; d++) {
                exact &= blend_channel(state, false, s, d) == logic_reference(op, s, d) && blend_channel(state, true, s, d) == logic_reference(op, s, d);
            }
        }
        CHECK(exact == logic_op_exact(op));
        BlendState constant = blend_state(mode, true, true, true);
        CHECK(!constant.enable || (constant.source_alpha == BlendFactor::One && constant.destination_alpha == BlendFactor::Zero &&
                                   constant.alpha_operation == BlendOperation::Add));
    }
    CHECK(logic_op_exact(0) && logic_op_exact(3) && logic_op_exact(5) && logic_op_exact(10) && logic_op_exact(12) && logic_op_exact(15));
    CHECK(!logic_op_exact(1) && !logic_op_exact(6) && !logic_op_exact(7) && !logic_op_exact(14));
    CHECK(std::string(logic_op_name(6)) == "xor");

    uint32_t all = 1 | 2 | (1u << 11) | (1u << 3) | (1u << 4) | (6u << 12);
    BlendState subtract = blend_state(all, true, false, true);
    CHECK(subtract.enable && !subtract.logic_op && subtract.operation == BlendOperation::ReverseSubtract);
    CHECK(subtract.source == BlendFactor::One && subtract.destination == BlendFactor::One);
    BlendState subtract_alone = blend_state((1u << 11) | (1u << 3), true, false, true);
    CHECK(!subtract_alone.enable && !subtract_alone.logic_op);
    BlendState subtract_logic = blend_state(2 | (1u << 11) | (1u << 3) | (6u << 12), true, false, true);
    CHECK(subtract_logic.logic_op && subtract_logic.logic_mode == 6 && subtract_logic.operation == BlendOperation::Add);
    BlendState subtract_constant = blend_state(all, true, true, true);
    CHECK(subtract_constant.alpha_operation == BlendOperation::Add && subtract_constant.destination_alpha == BlendFactor::Zero);

    uint32_t blend = 1 | 2 | (1u << 3) | (1u << 4) | (4u << 8) | (5u << 5) | (6u << 12);
    BlendState alpha_blend = blend_state(blend, true, false, true);
    CHECK(alpha_blend.enable && !alpha_blend.logic_op && alpha_blend.operation == BlendOperation::Add);
    CHECK(alpha_blend.source == BlendFactor::TevAlpha && alpha_blend.destination == BlendFactor::InverseTevAlpha);
    CHECK(alpha_blend.source_alpha == BlendFactor::TevAlpha && alpha_blend.destination_alpha == BlendFactor::InverseTevAlpha);
    CHECK(alpha_blend.output == LogicSource::Tev);
    BlendState color_blend = blend_state(1 | (1u << 3) | (1u << 4) | (2u << 8) | (3u << 5), true, false, true);
    CHECK(color_blend.source == BlendFactor::DestinationColor && color_blend.destination == BlendFactor::InverseSourceColor);
    CHECK(color_blend.source_alpha == BlendFactor::DestinationAlpha && color_blend.destination_alpha == BlendFactor::InverseTevAlpha);
    BlendState no_alpha = blend_state(1 | (1u << 3) | (1u << 4) | (6u << 8) | (7u << 5), false, false, true);
    CHECK(no_alpha.source == BlendFactor::One && no_alpha.destination == BlendFactor::Zero && !no_alpha.alpha_update);
    BlendState constant_blend = blend_state(blend, true, true, true);
    CHECK(constant_blend.source_alpha == BlendFactor::One && constant_blend.destination_alpha == BlendFactor::Zero);
    BlendState failing = blend_state(blend, true, false, false);
    CHECK(!failing.color_update && !failing.alpha_update);
    BlendState disabled = blend_state((1u << 3) | (1u << 4) | (4u << 8), true, false, true);
    CHECK(!disabled.enable && !disabled.logic_op && disabled.output == LogicSource::Tev);
}

void test_gx_line_point_offsets() {
    using namespace wp::gx;
    uint32_t bp[256] = {};
    CHECK(line_point_offsets(bp).line_coordinates == 0 && line_point_offsets(bp).point_coordinates == 0);
    bp[0x30] = (1u << 18) | 0xFF;
    bp[0x32] = 1u << 19;
    bp[0x34] = 3u << 18;
    bp[0x31] = 3u << 18;
    CHECK(line_point_offsets(bp).line_coordinates == 0 && line_point_offsets(bp).point_coordinates == 0);
    bp[0x22] = (3u << 16) | (5u << 19) | 0x1234;
    LinePointOffsets offsets = line_point_offsets(bp);
    CHECK(offsets.line == 0.25f && offsets.point == 1.0f);
    CHECK(offsets.line_coordinates == 5 && offsets.point_coordinates == 6);
    const float expected[8] = {0.0f, 1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 2.0f, 1.0f, 1.0f, 1.0f};
    for (uint32_t code = 0; code < 8; code++) {
        bp[0x22] = (code << 16) | (code << 19);
        CHECK(line_point_offsets(bp).line == expected[code] && line_point_offsets(bp).point == expected[code]);
    }
    CHECK(line_offset_negative_side(0.5f, 0.1f, false));
    CHECK(!line_offset_negative_side(-0.5f, 0.1f, false));
    CHECK(line_offset_negative_side(0.1f, -0.5f, true));
    CHECK(!line_offset_negative_side(0.1f, 0.5f, true));
    ScreenVertex vertex{};
    vertex.uv[0][0] = 0.5f;
    vertex.uv[2][1] = 0.25f;
    offset_texture_coordinates(vertex, 5, 0.125f, 0.5f);
    CHECK(vertex.uv[0][0] == 0.625f && vertex.uv[0][1] == 0.5f);
    CHECK(vertex.uv[1][0] == 0.0f && vertex.uv[1][1] == 0.0f);
    CHECK(vertex.uv[2][0] == 0.125f && vertex.uv[2][1] == 0.75f);
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
    test_gx_logic_ops();
    test_gx_line_point_offsets();
    std::free(wp::g_memory);
    if (failures == 0) {
        std::puts("all runtime tests passed");
    }
    return failures == 0 ? 0 : 1;
}
