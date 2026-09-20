# Wii Party Recomp

Static recompilation of Wii Party for native PC.

> **Status:** work in progress. The game reaches and draws its first screen but is not yet playable.

## About

Wii Party Recomp is a project that aims to translate the original game executable into C++ source code that can be compiled natively, so the game can run on PC without an emulator.

## Current state

The game runs natively on Windows up to its first screen. The recompiled program opens a window and draws the wrist strap warning with the game's own textures, at the display aspect ratio chosen by the virtual console settings (16:9 by default). The game does not go further yet because controller input is not implemented.

Working today:

- **Recompilation:** the main executable (about 7350 functions) and all 115 REL modules are translated to C++ and compile into one native program of roughly 300 MB. The translator is covered by unit tests, and one game function (a red-black tree erase) is checked against the C++ standard library.
- **Boot and system:** the Revolution OS initialisation, threads (as Windows fibers, including the game's own `setjmp`/`longjmp` coroutines), video retrace interrupts derived from the video mode, module loading and linking.
- **Disc and storage:** disc reads served from the extracted game files, IOS requests emulated at request level, and a virtual NAND with a default `SYSCONF` (English, widescreen).
- **Graphics:** the GX command stream is decoded and rendered with Direct3D 11: vertex formats, transforms, an ubershader for the TEV stages, texture decoding (I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, CMPR), blending, depth, scissor and EFB copies to the framebuffer.
- **Window:** a resizable window that keeps the aspect ratio.
- **Diagnostics:** environment variables such as `WP_LOG_GX`, `WP_PROFILE`, `WP_WATCH`, `WP_DUMP` and `WP_SAVE_FRAME`, a per-thread call trace, and a GDB client (`tools/dolphin_gdb.py`) to compare against a real Dolphin.

Not implemented yet:

- Controller input (keyboard, generic gamepads and real Wii Remotes).
- Audio (DSP).
- Graphics: lighting, indirect textures, palette textures, texture copies from the EFB, lines and points, and internal resolution scaling.
- Higher frame rates, online play and quality-of-life options.

Progress notes and the list of goals are in `DECOMP_PROGRESS.md`.

## Setup

Requirements: Git, CMake, Ninja, a C++ compiler (GCC/MinGW-w64 or MSVC), Rust (for `nodtool`), Python 3, JDK 21+, [Ghidra](https://github.com/NationalSecurityAgency/ghidra/releases) 12.x unpacked into `ghidra/install/` and the Ghidra GameCube Loader extension (Apache-2.0, provides the `Gekko_Broadway` processor with paired singles) installed in Ghidra.

1. Install the disc tool:

   ```
   cargo install nodtool
   ```

2. Put your own dump of Wii Party in `game/` (ISO, WBFS, RVZ or CISO; e.g. `game/wiiparty.rvz`).
3. Extract it:

   ```
   nodtool extract game/wiiparty.rvz extracted
   ```

   The recompiler inputs are `extracted/sys/main.dol` and the LZ11-compressed modules in `extracted/files/rel/*.rel.lz`; assets live in the rest of `extracted/files/`.

4. Unpack the REL modules and import the DOL into Ghidra. Optionally, name the functions first: in Dolphin, run the game, use Symbols > Generate Symbols From > Signature Database, then save the symbol map as `reference/symbols/SUPP01.map`.

   ```
   python tools/unpack_rels.py
   python tools/import_map.py
   python tools/ghidra_import.py
   ```

   Unpacked modules go to `build/rel/`, the converted symbol names to `build/dolphin_symbols.csv` and the Ghidra project to `ghidra/projects/`. The symbol map stays local and is never committed.

5. Generate the C++ from the DOL, build it and run the tests:

   ```
   python tools/recomp.py
   cmake -S . -B build/out -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build/out
   ctest --test-dir build/out
   ```

   `tools/recomp.py` reads `analysis/dol_functions.csv` and writes the generated sources to `build/recomp/`. Game modules are translated separately with `python tools/recomp_rel.py boot menu` (or `--all`, which produces about 650 MB of C++ and takes several minutes to compile) into `build/rel_code/`; run `tools/recomp.py` again afterwards so the DOL provides every function the modules call.

   Run the result with `build/out/wiiparty extracted [seconds] [nand directory]`. It opens a window that shows the console framebuffer; `seconds` is an optional watchdog that stops the process after that long (0 or omitted means no limit) and `WP_HEADLESS=1` runs without a window. The virtual NAND (settings and saves) lives in `game/nand`.

6. Optional, to drive Ghidra from an MCP client: create the virtual environment and install the bridge.

   ```
   uv venv .venv
   uv pip install --python .venv/Scripts/python.exe -r tools/requirements.txt
   ```

   The [GhidraMCP](https://github.com/bethington/ghidra-mcp) 6.0.0 extension must be unpacked into `%APPDATA%\ghidra\ghidra_12.1.3_PUBLIC\Extensions\` and enabled in Ghidra. Its server listens on `http://127.0.0.1:8089`.

7. Optional, to compare against a real Dolphin: add `GDBPort = 2159` under `[General]` in Dolphin's `Dolphin.ini`, then drive it with the bundled GDB client:

   ```
   python tools/dolphin_gdb.py --launch "break 80069ee0" "continue 40" "regs pc lr r1 r3" "mem 80000000 32"
   ```

   Commands are `regs`, `mem`, `u32`, `write`, `break`, `unbreak`, `watch`, `rwatch`, `awatch`, `step`, `continue`, `halt`, `raw` and `sleep`. Dolphin accepts one client per boot, so each call starts a fresh emulator (`--launch`; `--keep` leaves it running). It expects `reference/dolphin/Dolphin.exe` and `game/wiiparty.rvz` unless `--dolphin` and `--game` are given.

`game/`, `extracted/` and `reference/` (optional local material such as an emulator and RAM dumps of your own copy) are ignored by Git and must never be committed.

## Legal notice

This project does not include any copyrighted assets, game code or binaries from Nintendo. You must provide your own legally obtained copy of Wii Party. This project is not affiliated with or endorsed by Nintendo.

## Contributing

Issues and pull requests are welcome. Please do not share or upload copyrighted game files anywhere in this repository, including issues and pull requests.

## License

MIT. See [LICENSE](LICENSE).
