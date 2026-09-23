import argparse
import csv
from pathlib import Path

from tables import Tables

ROOT = Path(__file__).resolve().parents[2]
UCODE_LIST = ROOT / "analysis" / "dsp_ucode.csv"
DOL = ROOT / "extracted" / "sys" / "main.dol"
OUTPUT = ROOT / "build" / "dsp_recomp"
IRAM_WORDS = 0x1000
EXCEPTION_VECTORS = [2 * i for i in range(1, 8)]


def checksum(data):
    value = 0
    for byte in data:
        value = ((value * 31) ^ byte) & 0xFFFFFFFF
    return value


def instruction_starts(words, tables):
    starts = []
    loop_ends = set()
    address = 0
    while address < len(words):
        inst = words[address]
        template = tables.template(inst)
        starts.append(address)
        if (inst & 0xFFE0) == 0x0060 or (inst & 0xFF00) == 0x1100:
            if address + 1 < len(words):
                loop_ends.add(words[address + 1])
        elif (inst & 0xFFE0) == 0x0040 or (inst & 0xFF00) == 0x1000:
            loop_ends.add((address + 1) & 0xFFFF)
        address += template.size
    return starts, loop_ends


def emit_instruction(address, words, tables, loop_ends):
    inst = words[address]
    template = tables.template(inst)
    following = address + template.size
    lines = [f"L_{address:04x}:"]
    lines.append(f"    if (cycles <= 0) {{ s.pc = 0x{address:04x}; return cycles; }}")
    lines.append("    cycles--;")
    lines.append(f"    if (s.exceptions) {{ s.pc = 0x{address:04x}; if (s.CheckExceptions()) goto exception; }}")
    if address in EXCEPTION_VECTORS:
        lines.append(f"E_{address:04x}:")
    lines.append(f"    s.pc = 0x{(address + 1) & 0xFFFF:04x};")
    if template.extended:
        lines.append(f"    in.{tables.ext_handler(inst)}(0x{inst:04x});")
    lines.append(f"    in.{tables.handler(inst)}(0x{inst:04x});")
    if template.extended:
        lines.append("    in.ApplyWriteBackLog();")
    lines.append(f"    if (s.pc != 0x{following & 0xFFFF:04x}) goto changed;")
    if ((following - 1) & 0xFFFF) in loop_ends:
        lines.append("    in.HandleLoop();")
        lines.append(f"    if (s.pc != 0x{following & 0xFFFF:04x}) goto dispatch;")
    return lines


def emit_function(name, words, tables):
    starts, loop_ends = instruction_starts(words, tables)
    entry = set(starts)
    out = [
        f"int {name}(DSP::Interpreter::Interpreter& in, DSP::SDSP& s, int cycles, bool& idle) {{",
        "    if (cycles <= 0 || (s.control_reg & DSP::CR_HALT)) return cycles;",
        "    goto select;",
        "changed:",
        "    if (s.GetAnalyzer().IsLoopEnd(static_cast<u16>(s.pc - 1))) in.HandleLoop();",
        "dispatch:",
        "    if (cycles <= 0 || (s.control_reg & DSP::CR_HALT)) return cycles;",
        "    if (s.GetAnalyzer().IsIdleSkip(s.pc)) { idle = true; return cycles; }",
        "select:",
        "    switch (s.pc) {",
    ]
    for address in starts:
        out.append(f"    case 0x{address:04x}: goto L_{address:04x};")
    out.append("    default: return cycles;")
    out.append("    }")
    out.append("exception:")
    out.append("    switch (s.pc) {")
    for vector in EXCEPTION_VECTORS:
        if vector in entry:
            out.append(f"    case 0x{vector:04x}: goto E_{vector:04x};")
    out.append("    default: cycles++; return cycles;")
    out.append("    }")
    for index, address in enumerate(starts):
        out.extend(emit_instruction(address, words, tables, loop_ends))
        following = address + tables.template(words[address]).size
        if following not in entry:
            out.append("    goto dispatch;")
    out.append("}")
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dol", default=str(DOL))
    parser.add_argument("--out", default=str(OUTPUT))
    args = parser.parse_args()
    tables = Tables()
    dol = Path(args.dol).read_bytes()
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    with UCODE_LIST.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            offset = int(row["dol_offset"], 16)
            size = int(row["size"], 16)
            data = dol[offset:offset + size]
            crc = checksum(data)
            if crc != int(row["checksum"], 16):
                raise SystemExit(f"{row['name']}: checksum {crc:08x} does not match {row['checksum']}")
            words = [int.from_bytes(data[i:i + 2], "big") for i in range(0, size, 2)][:IRAM_WORDS]
            function = f"run_{row['name']}"
            body = [
                '#include "Core/DSP/DSPAnalyzer.h"',
                '#include "Core/DSP/DSPCore.h"',
                '#include "Core/DSP/Interpreter/DSPInterpreter.h"',
                "",
                "namespace wp::dsp::generated {",
                "",
            ]
            body += emit_function(function, words, tables)
            body += ["", "}", ""]
            (out_dir / f"{row['name']}.cpp").write_text("\n".join(body), encoding="utf-8")
            entries.append((row["name"], crc, function))
            print(f"{row['name']}: {len(words)} words, checksum {crc:08x}")
    registry = [
        '#include "wp/dsp_translated.h"',
        "",
        "namespace wp::dsp::generated {",
        "",
    ]
    for _, _, function in entries:
        registry.append(f"int {function}(DSP::Interpreter::Interpreter& in, DSP::SDSP& s, int cycles, bool& idle);")
    registry += ["", "}", "", "namespace wp::dsp {", "", "const TranslatedCode g_translated_code[] = {"]
    for _, crc, function in entries:
        registry.append(f"    {{0x{crc:08x}u, &generated::{function}}},")
    registry += ["};", "", "const size_t g_translated_code_count = sizeof(g_translated_code) / sizeof(g_translated_code[0]);", "", "}", ""]
    (out_dir / "registry.cpp").write_text("\n".join(registry), encoding="utf-8")


if __name__ == "__main__":
    main()
