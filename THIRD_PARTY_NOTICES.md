# Third-party notices and provenance

The original code of this project is licensed under GPL-3.0-or-later (see `LICENSE`). The GPL does not apply to, and does not grant rights over, Nintendo's game data.

## Status

Adapted third-party components are included (see below). This file records what was reviewed and what is still pending. It is not a legal audit.

## Third-party code included in the repository

### GX lighting from the Dolphin Emulator software renderer

- Repository files: `src/gx_lighting.cpp` and `include/wp/gx_lighting.h`. They are kept apart from the project's own code and carry the upstream copyright line and SPDX identifier.
- Upstream file: `Source/Core/VideoBackends/Software/TransformUnit.cpp` of the Dolphin Emulator, from a source snapshot of version 2606 (taken from `CMake/ScmRevGen.cmake` in the local copy under `reference/dolphin-source`; the snapshot has no Git metadata, so the exact commit is not known).
- Parts adapted: `TransformNormal`, `SafeDivide`, `CalculateLightAttn`, `LightColor`, `LightAlpha` and the channel logic of `TransformColor`. The register layout was read from `Source/Core/VideoCommon/XFMemory.h` (same snapshot); no code from that file was copied.
- Copyright: "Copyright 2009 Dolphin Emulator Project", as stated in the upstream file header.
- License: `SPDX-License-Identifier: GPL-2.0-or-later`, as stated in the upstream file header. Dolphin's `COPYING` and `LICENSES/GPL-2.0-or-later.txt` confirm it. GPL-2.0-or-later code may be used under GPL-3.0-or-later, so it is compatible with this project.
- Dependencies: none copied. Dolphin's `Common::Vec3`, logging, assertion and memory types were replaced by small local equivalents; no other Dolphin file is included.
- Changes (2026-09-22, this project): reads lights and channel registers from this project's flat XF register array instead of Dolphin's `XFMemory` struct; works on RGBA bytes instead of Dolphin's ABGR layout; removes the panic alerts; returns the normal instead of dividing by zero when its length is zero; exposes two functions (`transform_normal`, `light_channels`) used by `src/gx.cpp`.

### Emulated Bluetooth controller from the Dolphin Emulator

- Repository files: `src/bluetooth.cpp` and `include/wp/bluetooth.h`, kept apart from the project's own code, with the upstream copyright line and SPDX identifier.
- Upstream file: `Source/Core/Core/IOS/USB/Bluetooth/BTEmu.cpp` (same 2606 snapshot as above, exact commit unknown), plus the USB V0 message layout from `Source/Core/Core/IOS/USB/USBV0.cpp` and the buffer constants from `BTBase.h`.
- Parts adapted: the USB V0 control, bulk and interrupt message handling, the HCI command replies (reset, buffer size, local version and features, controller address, stored link keys, scan enable, supervision timeout, vendor commands), the event queue and the pending event and ACL endpoints.
- Copyright: "Copyright 2008 Dolphin Emulator Project" (upstream header). License: GPL-2.0-or-later (SPDX identifier in the upstream files), compatible with GPL-3.0-or-later.
- Not copied: `hci.h` and `l2cap.h`, which mix Dolphin code with BSD-licensed code from NetBSD and FreeBSD. The HCI opcodes and event codes used here were written from the Bluetooth specification.
- Also adapted, from `Source/Core/Core/IOS/USB/Bluetooth/WiimoteDevice.cpp` ("Copyright 2008 Dolphin Emulator Project", GPL-2.0-or-later): the connection sequence of an emulated Wii Remote (connection request, accept, remote name, features, authentication) and the L2CAP signalling for the HID control and interrupt channels (connection, configuration, disconnection).
- Changes (2026-09-23, this project): rewritten around this project's IOS request handling (deferred replies taken by `ios::take_completion`), byte vectors instead of packed structs, no save states, one Wii Remote attached, channels opened once the host has configured the link.

### Emulated Wii Remote from the Dolphin Emulator

- Repository files: `src/wiimote.cpp` and `include/wp/wiimote.h`, kept apart from the project's own code, with the upstream copyright lines and SPDX identifier.
- Upstream files (same 2606 snapshot, exact commit unknown): `Source/Core/Core/HW/WiimoteEmu/EmuSubroutines.cpp` and `WiimoteEmu.cpp` ("Copyright 2010 Dolphin Emulator Project"), and `Camera.cpp` ("Copyright 2019 Dolphin Emulator Project"). Report layouts were read from `WiimoteCommon/WiimoteReport.h` and `WiimoteConstants.h`; no code from those headers was copied.
- Parts adapted: the output report handling (LEDs, report mode, IR camera enable, status request, memory and register read and write, acknowledgements), the EEPROM contents with calibration data, the I2C register spaces of the IR camera and speaker, the data report layouts (buttons, accelerometer, IR in basic, extended and full formats, extension bytes) and the 200 Hz report timing.
- License: `SPDX-License-Identifier: GPL-2.0-or-later` in every upstream file, compatible with GPL-3.0-or-later.
- Not copied: the motion simulation (`Dynamics.cpp`), extensions, MotionPlus, the speaker decoder and the encryption code. The mapping from the PC pointer to the two IR dots is this project's own: it was measured against the game's own KPAD so that KPAD reports the same position as the input.
- Changes (2026-09-23, this project): reads a simple input sample (buttons and pointer) instead of Dolphin's input configuration; sends reports through a callback into `src/bluetooth.cpp`; no save states.

### EFB clear rules from the Dolphin Emulator

- Repository file: `src/gx_render.cpp` (`clear`), own code.
- Behaviour studied in the Dolphin Emulator's `Source/Core/VideoCommon/BPFunctions.cpp` (`ClearScreen`), `FramebufferManager.cpp` (`FramebufferManager::ClearEFB`) and `Source/Core/Common/ColorUtil.h` (`RGBA8ToRGBA6ToRGBA8`, `RGBA8ToRGB565ToRGBA8`, `Z24ToZ16ToZ24`), current `master` read on 2026-09-24 ("Copyright 2009 Dolphin Emulator Project" and later years, GPL-2.0-or-later). No code was copied: the rules (clear only the copied rectangle and only the channels whose update is enabled; formats without alpha force an alpha clear to 0; RGBA6 and RGB565 color and Z16 depth rounding) are reimplemented, and the three one-line bit conversions are written as equivalent expressions.

### DSP core and free DSP ROMs from the Dolphin Emulator

- Repository files: everything under `third_party/dolphin/`, kept in Dolphin's own directory layout so it stays separate from this project's code. `third_party/dolphin/COPYING` and `third_party/dolphin/LICENSES/GPL-2.0-or-later.txt` are copied from the same snapshot.
- Upstream files (2606 snapshot, exact commit unknown): `Source/Core/Core/DSP/DSPCore.*`, `DSPTables.*`, `DSPAccelerator.*`, `DSPHWInterface.cpp`, `DSPMemoryMap.cpp`, `DSPStacks.cpp`, `DSPAnalyzer.*`, `DSPHost.h`, `DSPCommon.h`, `DSPBreakpoints.h`, `DSPCaptureLogger.h`, all of `Source/Core/Core/DSP/Interpreter/`, and `Source/Core/Common/CommonTypes.h`, `BitField.h`, `Inline.h`.
- Copyright, as stated in each file header: "Copyright 2008/2009/2010/2014/2018 Dolphin Emulator Project", and in some files also "Copyright 2004 Duddie & Tratax" or "Copyright 2005 Duddie". Every header carries `SPDX-License-Identifier: GPL-2.0-or-later`, compatible with GPL-3.0-or-later.
- `third_party/dolphin/Data/Sys/GC/dsp_rom.bin` and `dsp_coef.bin`: Dolphin's free replacements for the DSP instruction ROM and coefficient ROM (version 0.4, written by Dolphin contributors: duddie, LM, ligfx, Tilka, Pokechu22 and others; history and source in `docs/DSP/free_dsp_rom/` of the snapshot). They contain no Nintendo code. They carry no per-file notice, so they fall under the repository licence, GPL-2.0-or-later (`COPYING` of the snapshot). They are embedded in the executable as resources 2 and 3 (`res/wiiparty.rc`).
- Changes (2026-09-23, this project), all in `third_party/dolphin/`: the DSP JIT was removed from `DSPCore.cpp`/`DSPCore.h` (only the interpreter is used); `DSPInterpreter.cpp` reads time through a new `DSP::Host::TimeBase()` (declared in `DSPHost.h`) instead of Dolphin's `Core::System` timers; `DSPTables.cpp` formats one hex string with `snprintf` instead of `fmt`; `DSPAccelerator.cpp` includes `Common/MathUtil.h`; the `fmt::formatter` specialisations and the `fmt` include were removed from `BitField.h`. `Interpreter::HandleLoop` was moved from the private to the public part of `DSPInterpreter.h` so the recompiled microcode can call it.
- Not copied: the DSP JIT, assembler, disassembler, capture logger implementation, symbols, and Dolphin's `Core::System`, `CoreTiming`, `Memmap` and logging. `third_party/dolphin_shim/` holds small replacements written for this project (no-op logging and assertions, a mutex-based `Common::Event`, page allocation with `calloc`, an Adler-32 hash, `MathUtil::SaturatingCast` and a `PointerWrap` that serialises into a byte vector, used by `WP_DSP_VERIFY`), so the Dolphin files compile unchanged otherwise.
- `src/dsp.cpp` and `include/wp/dsp.h` (Dolphin copyright line and SPDX identifier): the CPU side of the DSP adapted from Dolphin's `Source/Core/Core/HW/DSP.cpp` (DSP interface registers, control register, ARAM DMA, audio DMA and its interrupts, "Copyright 2008 Dolphin Emulator Project"), `HW/DSPLLE/DSPLLE.cpp` (mailbox and control register glue, the DSP thread, "Copyright 2008") and `HW/DSPLLE/DSPHost.cpp` (the `DSP::Host` functions, "Copyright 2009"), all GPL-2.0-or-later. Rewritten for this project's memory and interrupt model: registers are reached through the `0xCC005xxx` check in `include/wp/memory.h`, interrupts are delivered by `src/interrupts.cpp`, the DSP runs in lockstep on the CPU thread (as in Dolphin's single-core LLE mode, without its thread), and audio DMA blocks go to `src/audio.cpp` (own code, WASAPI output).

- `tools/dsp/tables.py` (own code) reads the opcode templates and interpreter handler tables from `third_party/dolphin/Source/Core/Core/DSP/DSPTables.cpp` and `Interpreter/DSPIntTables.cpp`, and the idle-skip signatures from `DSPAnalyzer.cpp`, at build time; nothing from those files is copied into the tool. The C++ generated from the microcode uses the helpers below, calls the Dolphin interpreter for the few opcodes that are not inline, and is not committed (it is derived from the game's microcode).
- `include/wp/dsp_inline.h` and `tools/dsp/inline_ops.py` (Dolphin copyright lines and SPDX identifier, GPL-2.0-or-later): inline versions of the DSP interpreter, adapted from Dolphin's `Source/Core/Core/DSP/Interpreter/DSPInterpreter.cpp` (register access, address register arithmetic, accumulator and product helpers, status register updates and condition codes), `DSPIntCCUtil.h`, `DSPIntUtil.h`, `DSPIntArithmetic.cpp`, `DSPIntMultiplier.cpp`, `DSPIntLoadStore.cpp`, `DSPIntMisc.cpp`, `DSPIntBranch.cpp` (branches and `HandleLoop`) and `DSPIntExtOps.cpp` ("Copyright 2008 Dolphin Emulator Project, Copyright 2004 Duddie & Tratax" in `DSPInterpreter.cpp`, "Copyright 2005 Duddie" in `DSPIntUtil.h`, "Copyright 2009 Dolphin Emulator Project" in the others; all four lines are kept in both files). The header holds the helpers as `always_inline` functions over the public `SDSP` state; the Python module emits, for each supported opcode, the statements of the matching interpreter handler with the operand fields already decoded. Written for this project: the status register updates are computed without branches and, where the next instructions overwrite them before anything reads them, only when the translated code is left (same result, checked by `WP_DSP_VERIFY`).

## Rules for adding third-party code

Before any external file is added it must have, in this file:

1. The exact files reused, their upstream path and the upstream revision.
2. The authors and copyright notices, kept intact.
3. The exact license of each file (a repository can mix licenses).
4. Its dependencies and a check that they are compatible with GPL-3.0-or-later.
5. Where the files live in this repository, kept apart from original code whenever reasonable.

If the license or origin of a file is unclear, it is not copied.

## Projects reviewed as possible sources

Nothing from these has been copied. License information was read from the files kept in the local, untracked `reference/` folder.

| Project | License as stated by the project | Status |
| --- | --- | --- |
| Dolphin Emulator | Its `COPYING` says most original code is GPLv2+, that parts derive from other projects with stronger or weaker terms, and that the whole is compatible with GPLv3. | `TransformUnit.cpp` audited and adapted (see above). The rest is not audited file by file. |
| WiiCompiled (Mario Kart Wii) | GPLv3 (`LICENSE`), with its own `THIRD-PARTY-NOTICES.md` listing bundled components. | Identified. Not audited file by file. |
| Aurora | MIT (license file present in the WiiCompiled tree that vendors it). Aurora itself vendors other components with their own licenses. | Identified. Not audited. |
| RecompCore | Described as a Dolphin fork; its license files have not been read. | Identified only. License unverified. |

## Tools used but not distributed

Ghidra, Dolphin, nodtool, dtk, CMake, Ninja, GCC (MinGW-w64), Python and its packages listed in `tools/requirements.txt` are used to build or analyse the project. None is included in the repository. Their licenses apply to their own use.

## Items in this repository pending review

- `analysis/dol_functions.csv`: function addresses and sizes obtained by analysing the user's own `main.dol`. It contains no game code, but it is derived from game data.
- `analysis/symbols.csv` and `analysis/hle_functions.csv`: symbol names for SDK functions. Written during this project; part of the names were checked against a symbol map exported from Dolphin, which is kept local and not committed. Their origin has not been audited.
- `tools/lz11.py`, `tools/ppc/decoder.py` and other tools: written for this project. No known third-party code, but not audited.
- Code in this repository was written with the help of an AI assistant. Whether and how that affects copyright is a question for legal advice.

## Prohibited material

Not to be used, even though the project is GPL:

- Code from InputEvelution/wp without a valid authorization or license.
- Leaked, proprietary, unauthorised or unknown-origin code.
- Original Wii Party data: `main.dol`, `.rel` files, ISO or parts of it, textures, models, sounds, fonts, videos and any other protected resource.

`game/`, `extracted/` and `reference/` stay ignored by Git.
