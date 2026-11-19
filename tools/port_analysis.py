#!/usr/bin/env python3
"""Carries the DOL analysis data of one disc version over to another build of the same executable.

    python tools/port_analysis.py <reference main.dol> <reference analysis folder> <target main.dol> <output folder>

Regional builds of Wii Party share almost all of their code and differ by blocks that were added or
removed, so every function keeps its instructions and only moves by a few offsets. Each function of the
reference is looked up in the target by its instructions with the address fields masked; the offset that
matches best is kept. The output folder gets dol_functions.csv, symbols.csv and dsp_ucode.csv for the
target, and the script stops if a named OS or SDK function does not match exactly.
"""
import csv
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "recompiler"))
import dol as dol_module

MIN_SIMILARITY = 0.6
SEARCH_WORDS = 64
PREFIX = 6


def mask(word):
    op = word >> 26
    if op == 18:
        return word & 0xFC000000
    if op == 16:
        return word & 0xFFFF0000
    if 14 <= op <= 15 or op == 24 or op == 25 or 32 <= op <= 55:
        return word & 0xFFFF0000
    return word


def masked_words(dol, address, size):
    words = []
    for cursor in range(address, address + size, 4):
        word = dol.word_at(cursor)
        if word is None:
            break
        words.append(mask(word))
    return words


def similarity(left, right):
    if not left or len(left) != len(right):
        return 0.0
    return sum(1 for a, b in zip(left, right) if a == b) / len(left)


def read_csv(path):
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def build_index(target):
    index = {}
    for section in target.sections:
        if not section.executable:
            continue
        data = target.section_bytes(section)
        words = [mask(w) for w in struct.unpack(">%dI" % (len(data) // 4), data[: len(data) // 4 * 4])]
        for position in range(len(words) - PREFIX):
            index.setdefault(tuple(words[position : position + PREFIX]), []).append(section.address + 4 * position)
    return index


def unique_offsets(functions, reference, target, index):
    found = {}
    for address, size in functions:
        words = masked_words(reference, address, size)
        if len(words) < PREFIX + 2:
            continue
        matches = [c for c in index.get(tuple(words[:PREFIX]), []) if masked_words(target, c, size) == words]
        if len(matches) == 1:
            found[address] = matches[0] - address
    return found


def place(address, size, reference, target, index, offsets):
    words = masked_words(reference, address, size)
    if not words:
        return None, 0.0
    best = (None, 0.0)
    for offset in offsets:
        score = similarity(words, masked_words(target, address + offset, size))
        if score > best[1]:
            best = (address + offset, score)
    if best[1] >= 1.0 or len(words) < PREFIX:
        return best
    near = [c for c in index.get(tuple(words[:PREFIX]), []) if abs(c - (best[0] or address)) <= SEARCH_WORDS * 4]
    for candidate in near:
        score = similarity(words, masked_words(target, candidate, size))
        if score > best[1]:
            best = (candidate, score)
    return best


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    reference = dol_module.load(sys.argv[1])
    analysis = Path(sys.argv[2])
    target = dol_module.load(sys.argv[3])
    output = Path(sys.argv[4])
    output.mkdir(parents=True, exist_ok=True)
    functions = sorted((int(row["address"], 16), int(row["size"])) for row in read_csv(analysis / "dol_functions.csv"))
    index = build_index(target)
    offsets = sorted(set(unique_offsets(functions, reference, target, index).values()), key=lambda value: value)
    moved = {}
    dropped = []
    changed = 0
    for address, size in functions:
        spot, score = place(address, size, reference, target, index, offsets)
        if spot is None or score < MIN_SIMILARITY:
            dropped.append(address)
            continue
        if score < 1.0:
            changed += 1
        moved[address] = (spot, size)
    names = read_csv(analysis / "symbols.csv")
    symbols = []
    for row in names:
        address = int(row["address"], 16)
        if row["name"].startswith("HleZero_") or address not in moved:
            spot, score = place(address, 4 * 8, reference, target, index, offsets)
        else:
            spot, score = moved[address][0], 1.0
            size = dict(functions)[address]
            exact = similarity(masked_words(reference, address, size), masked_words(target, spot, size))
            score = exact
        if spot is None or (score < 1.0 and not row["name"].startswith("HleZero_")):
            raise SystemExit(f"{row['name']} at {address:#x} does not match exactly in the target (best {score:.2f})")
        symbols.append((spot, row["name"]))
    with open(output / "dol_functions.csv", "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["address", "size"])
        for spot, size in sorted(moved.values()):
            writer.writerow([f"0x{spot:08x}", size])
    with open(output / "symbols.csv", "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["address", "name"])
        for spot, name in symbols:
            writer.writerow([f"0x{spot:08x}", name])
    ucode_rows = read_csv(analysis / "dsp_ucode.csv")
    with open(output / "dsp_ucode.csv", "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["name", "dol_offset", "size", "checksum"])
        for row in ucode_rows:
            offset = int(row["dol_offset"], 16)
            size = int(row["size"], 16)
            blob = reference.data[offset : offset + size]
            where = target.data.find(blob)
            if where < 0 or target.data.find(blob, where + 1) >= 0:
                raise SystemExit(f"microcode {row['name']} is not found exactly once in the target")
            writer.writerow([row["name"], f"{where:x}", row["size"], row["checksum"]])
    print(f"functions carried over {len(moved)} of {len(functions)} ({changed} with changes, {len(dropped)} dropped)")
    print("offsets", Counter(spot - address for address, (spot, _) in moved.items()).most_common())
    sdk = {row["name"]: row["address"] for row in names}
    mapped = {name: spot for spot, name in symbols}
    print("OSSaveContext", f"0x{mapped['OSSaveContext']:08x}", "setjmp", f"0x{mapped['setjmp']:08x}", "(reference", sdk["OSSaveContext"], sdk["setjmp"] + ")")
    if dropped:
        print("dropped", " ".join(f"0x{a:08x}" for a in dropped[:40]))


if __name__ == "__main__":
    main()
