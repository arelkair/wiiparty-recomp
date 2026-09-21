# Wii Party Recomp

Static recompilation of Wii Party for native PC.

> **Status:** work in progress. The game boots, reaches the menu and plays through the start of Board Game Island, including a minigame, but it is not yet playable to the end of a turn.

![Project progress](docs/progress.svg)

## About

Wii Party Recomp is a project that aims to translate the original game executable into C++ source code that can be compiled natively, so the game can run on PC without an emulator.

## Current state

The game runs natively on Windows at a steady 50 fps (PAL), with keyboard and mouse input. Checked by running the game with scripted input (`WP_INPUT_SCRIPT`): title, main menu, Board Game Island, number of players, Mii selection, CPU skill, the host explanation, the "Maze Daze" instruction screen, the minigame, the play order and the board turn screen.

### Progress

The overall figure is the equal-weight average of the components below. They are estimates, not measurements. The values live in `analysis/progress.csv` and `python tools/progress.py` regenerates the image.

| Component | Progress | Basis |
| --- | --- | --- |
| Recompilation toolchain | 95% | DOL and all 115 modules translate and build; module fixes still turn up and per-module correctness is unverified |
| System runtime | 85% | OS, threads, interrupts, decrementer, IOS, DVD and NAND work; free of known blockers except finished fibers not freed |
| Graphics (GX to Direct3D 11) | 65% | Menus, text, 3D models and Miis draw; no lighting, indirect textures, mipmaps or R8 copies; menu panel colors differ |
| Input | 40% | Keyboard and mouse as a Wii Remote; no gamepads or real Wii Remotes |
| Audio | 0% | AX sound layer is stubbed; the game is silent |
| Game flow | 35% | Reaches title, menu, Board Game Island, a minigame and the board turn; freezes when the first Mii moves |
| PC features | 10% | Native resolution multiplier and 4:3 window only; launcher, options menu, ultrawide and online not started |

### Working

- **Recompilation:** the main executable (about 7,350 functions) and all 115 REL modules are translated to C++ and build into one native program of roughly 300 MB. Unit tests cover the translator; one game function is checked against the C++ standard library.
- **System:** Revolution OS initialisation, locked cache DMA, threads as Windows fibers (including the game's own `setjmp`/`longjmp` coroutines), video retrace and decrementer interrupts, OS alarms, module loading and linking, disc reads from the extracted files, IOS at request level and a virtual NAND. Real Mii databases (`RFL_DB.dat`) load.
- **Graphics:** the GX command stream is decoded and drawn with Direct3D 11: vertex formats, transforms, a TEV ubershader with channel swap tables, display lists, draw batching, indexed skeleton matrices, texture decoding (I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, C4, C8, C14X2, CMPR), blending, depth, scissor and EFB copies to textures.
- **Window:** 4:3, native internal resolution by default with an integer multiplier (`WP_SCALE`, 1 to 6), presented through a DXGI swap chain.
- **Diagnostics:** `WP_LOG_GX`, `WP_LOG_FPS`, `WP_LOG_IOS`, `WP_LOG_IRQ`, `WP_LOG_INPUT`, `WP_LOG_DISC`, `WP_PROFILE`, `WP_WATCH`, `WP_DUMP`, `WP_SAVE_FRAME`, a crash reporter with guest registers and call stacks, and a GDB client for Dolphin (`tools/dolphin_gdb.py`).

### Known problems

- The game freezes when the first Mii moves on the board. An object rotation angle grows without limit; the cause is under investigation.
- The minigame is drawn incorrectly (upside down or black) and the barrel of the first minigame does not appear.
- The dice show no pips.
- Miis look flat: GX lighting is not implemented.
- Text such as "Round 1" is not drawn.
- The pictures in the board explanation are black rectangles.
- Menu panel colors differ from the real game (blue instead of red, purple instead of orange). The game chooses a different color id; the source of that id is not found yet.
- `unsupported EFB copy format 8` (R8 EFB copy) is logged.

### Not implemented

- Audio: the AX sound layer is stubbed, so the game is silent.
- Generic gamepads and real Wii Remotes.
- GX: lighting, indirect textures, mipmaps, lines and points, EFB copies in intensity formats.
- Planned: ultrawide display support, higher frame rates, a launcher and options menu, online play, quality-of-life options and a Galician translation.

Progress notes and the list of goals are in `DECOMP_PROGRESS.md`; planned features are in `FEATURES_QOL.md`.

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

Releasing the project's own code under the GPL does not make Nintendo's original resources (executables, textures, models, sounds, fonts, videos) redistributable, and it grants no rights over them.

## Contributing

Issues and pull requests are welcome. Please do not share or upload copyrighted game files anywhere in this repository, including issues and pull requests.

## License

The original code of this project is licensed under the GNU General Public License, version 3 or (at your option) any later version (GPL-3.0-or-later). See [LICENSE](LICENSE). Copyright (C) 2026 Arel Kair.

The project may incorporate third-party components under their own licenses, which must be respected. [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) records what has been reviewed and what is still pending; no third-party source code is included yet and no complete license audit has been done.
