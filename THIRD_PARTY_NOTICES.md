# Third-party notices and provenance

The original code of this project is licensed under GPL-3.0-or-later (see `LICENSE`). The GPL does not apply to, and does not grant rights over, Nintendo's game data.

## Status

Adapted third-party components are included (see below). This file records what was reviewed and what is still pending. It is not a legal audit.

## Third-party code included in the repository

### GX lighting from the Dolphin Emulator software renderer

- Repository files: `engine/src/gpu/gx_lighting.cpp` and `engine/include/wp/gx_lighting.h`. They are kept apart from the project's own code and carry the upstream copyright line and SPDX identifier.
- Upstream file: `Source/Core/VideoBackends/Software/TransformUnit.cpp` of the Dolphin Emulator, from a source snapshot of version 2606 (taken from `CMake/ScmRevGen.cmake` in the local copy under `reference/dolphin-source`; the snapshot has no Git metadata, so the exact commit is not known).
- Parts adapted: `TransformNormal`, `SafeDivide`, `CalculateLightAttn`, `LightColor`, `LightAlpha` and the channel logic of `TransformColor`. The register layout was read from `Source/Core/VideoCommon/XFMemory.h` (same snapshot); no code from that file was copied.
- Copyright: "Copyright 2009 Dolphin Emulator Project", as stated in the upstream file header.
- License: `SPDX-License-Identifier: GPL-2.0-or-later`, as stated in the upstream file header. Dolphin's `COPYING` and `LICENSES/GPL-2.0-or-later.txt` confirm it. GPL-2.0-or-later code may be used under GPL-3.0-or-later, so it is compatible with this project.
- Dependencies: none copied. Dolphin's `Common::Vec3`, logging, assertion and memory types were replaced by small local equivalents; no other Dolphin file is included.
- Changes (2026-09-22, this project): reads lights and channel registers from this project's flat XF register array instead of Dolphin's `XFMemory` struct; works on RGBA bytes instead of Dolphin's ABGR layout; removes the panic alerts; returns the normal instead of dividing by zero when its length is zero; exposes two functions (`transform_normal`, `light_channels`) used by `engine/src/gpu/gx.cpp`.

### Emulated Bluetooth controller from the Dolphin Emulator

- Repository files: `engine/src/input/bluetooth.cpp` and `engine/include/wp/bluetooth.h`, kept apart from the project's own code, with the upstream copyright line and SPDX identifier.
- Upstream file: `Source/Core/Core/IOS/USB/Bluetooth/BTEmu.cpp` (same 2606 snapshot as above, exact commit unknown), plus the USB V0 message layout from `Source/Core/Core/IOS/USB/USBV0.cpp` and the buffer constants from `BTBase.h`.
- Parts adapted: the USB V0 control, bulk and interrupt message handling, the HCI command replies (reset, buffer size, local version and features, controller address, stored link keys, scan enable, supervision timeout, vendor commands), the event queue and the pending event and ACL endpoints.
- Copyright: "Copyright 2008 Dolphin Emulator Project" (upstream header). License: GPL-2.0-or-later (SPDX identifier in the upstream files), compatible with GPL-3.0-or-later.
- Not copied: `hci.h` and `l2cap.h`, which mix Dolphin code with BSD-licensed code from NetBSD and FreeBSD. The HCI opcodes and event codes used here were written from the Bluetooth specification.
- Also adapted, from `Source/Core/Core/IOS/USB/Bluetooth/WiimoteDevice.cpp` ("Copyright 2008 Dolphin Emulator Project", GPL-2.0-or-later): the connection sequence of an emulated Wii Remote (connection request, accept, remote name, features, authentication) and the L2CAP signalling for the HID control and interrupt channels (connection, configuration, disconnection).
- Changes (2026-09-23, this project): rewritten around this project's IOS request handling (deferred replies taken by `ios::take_completion`), byte vectors instead of packed structs, no save states, one Wii Remote attached, channels opened once the host has configured the link.

### Emulated Wii Remote from the Dolphin Emulator

- Repository files: `engine/src/input/wiimote.cpp` and `engine/include/wp/wiimote.h`, kept apart from the project's own code, with the upstream copyright lines and SPDX identifier.
- Upstream files (same 2606 snapshot, exact commit unknown): `Source/Core/Core/HW/WiimoteEmu/EmuSubroutines.cpp` and `WiimoteEmu.cpp` ("Copyright 2010 Dolphin Emulator Project"), and `Camera.cpp` ("Copyright 2019 Dolphin Emulator Project"). Report layouts were read from `WiimoteCommon/WiimoteReport.h` and `WiimoteConstants.h`; no code from those headers was copied.
- Parts adapted: the output report handling (LEDs, report mode, IR camera enable, status request, memory and register read and write, acknowledgements), the EEPROM contents with calibration data, the I2C register spaces of the IR camera and speaker, the data report layouts (buttons, accelerometer, IR in basic, extended and full formats, extension bytes) and the 200 Hz report timing.
- License: `SPDX-License-Identifier: GPL-2.0-or-later` in every upstream file, compatible with GPL-3.0-or-later.
- Not copied: the motion simulation (`Dynamics.cpp`), extensions, MotionPlus, the speaker decoder and the encryption code. The mapping from the PC pointer to the two IR dots is this project's own: it was measured against the game's own KPAD so that KPAD reports the same position as the input.
- Changes (2026-09-23, this project): reads a simple input sample (buttons and pointer) instead of Dolphin's input configuration; sends reports through a callback into `engine/src/input/bluetooth.cpp`; no save states.

### EFB clear rules from the Dolphin Emulator

- Repository file: `engine/src/gpu/gx_render.cpp` (`clear`), own code.
- Behaviour studied in the Dolphin Emulator's `Source/Core/VideoCommon/BPFunctions.cpp` (`ClearScreen`), `FramebufferManager.cpp` (`FramebufferManager::ClearEFB`) and `Source/Core/Common/ColorUtil.h` (`RGBA8ToRGBA6ToRGBA8`, `RGBA8ToRGB565ToRGBA8`, `Z24ToZ16ToZ24`), current `master` read on 2026-09-24 ("Copyright 2009 Dolphin Emulator Project" and later years, GPL-2.0-or-later). No code was copied: the rules (clear only the copied rectangle and only the channels whose update is enabled; formats without alpha force an alpha clear to 0; RGBA6 and RGB565 color and Z16 depth rounding) are reimplemented, and the three one-line bit conversions are written as equivalent expressions.

### IPC hardware and IOS file system timing from the Dolphin Emulator

- Repository files: `engine/src/ios/ipc.cpp` (IPC registers `0xCD000000`-`0xCD000034`, control bits X1/X2/Y1/Y2/IY1/IY2, acknowledge and reply sequencing) and the timing model in `engine/src/ios/ios.cpp` (`lookup_ticks`, `memcpy_ticks`, `has_cache`, `flush_cache`, `populate_cache`, `read_write_ticks` and the latencies added in `send` and `fs_command`).
- Source: the Dolphin Emulator's `Source/Core/Core/HW/WII_IPC.cpp` and `WII_IPC.h` ("Copyright 2008 Dolphin Emulator Project"), `Source/Core/Core/IOS/IOS.cpp` and `IOS.h` (`EnqueueIPCRequest`, `EnqueueIPCReply`, `UpdateIPC`, `IPC_OVERHEAD_TICKS`, the default reply delay) and `Source/Core/Core/IOS/FS/FileSystemProxy.cpp` ("Copyright 2018 Dolphin Emulator Project"), current `master` read on 2026-09-24. License: GPL-2.0-or-later, compatible with this project's GPL-3.0-or-later.
- Adapted, not copied verbatim: the logic and the measured constants (500 timebase ticks to acknowledge a request, 100 ticks from Y1/Y2 to the interrupt, 2700 ticks of IPC overhead, 4000 ticks for a default reply, and for IOS 56 a superblock write of 3,170,000 ticks, a cluster write of 300,000, a cluster read of 115,000, `0.636 * size + 150` for cached copies, 1000 for a free-cluster check, 680 per path component for a lookup and `1000 + 340` per component for a split lookup) are rewritten for this runtime. Dolphin's scheduler is replaced by a small event list on the guest time base.

### Wii Remote motion defaults from the Dolphin Emulator

- Repository files: `update_motion` in `engine/src/input/wiimote.cpp` and the motion bindings in `engine/src/input/input.cpp`, own code.
- Behaviour studied in the Dolphin Emulator's `Source/Core/Core/HW/WiimoteEmu/Dynamics.cpp`, `WiimoteEmu.cpp` and `Source/Core/InputCommon/ControllerEmu/ControlGroup/Force.cpp` and `Tilt.cpp` ("Copyright 2019 Dolphin Emulator Project" and related years, GPL-2.0-or-later), current `master` read on 2026-09-24. No code was copied: the conventions (accelerometer at rest reads +1 g on Z, tilting right moves gravity to +X, 10-bit values from the calibration's zero and one-g points, clamped to 0-1023) and the defaults (shake on the middle mouse button, 10 cm of travel at 6 shakes per second on all three axes) are reimplemented with a simpler model.

### EFB copy conversion from the Dolphin Emulator

- Repository file: the `kCopyShaderSource` shader and `run_copy` in `engine/src/gpu/gx_render.cpp`, and `copy_filter` in `engine/src/gpu/gx.cpp`.
- Adapted from `Source/Core/VideoCommon/TextureConverterShaderGen.cpp` ("Copyright 2017 Dolphin Emulator Project", `SPDX-License-Identifier: GPL-2.0-or-later`), 2606 snapshot: the conversion of each copy format (R4, R8, RA4, RA8, RGB565, RGB5A3, A8, G8, B8, RG8, GB8), the intensity (YUV) constants, the three-row copy filter with its overflow rule, gamma, the alpha of EFB formats without alpha, and the split of the 24-bit depth into three bytes. The grouping of the seven filter weights, the gamma table and the clamp rows follow `TextureCacheBase.cpp` (`GetRAMCopyFilterCoefficients`, `CopyFilterCanOverflow`, `CopyEFBToCacheEntry`) and `BPStructs.cpp` of the same snapshot. Rewritten in HLSL for this project's Direct3D 11 renderer, reading the EFB colour and depth textures directly; GPL-2.0-or-later is compatible with GPL-3.0-or-later.
- Also from `Source/Core/VideoCommon/PixelShaderGen.cpp` ("Copyright 2008 Dolphin Emulator Project", GPL-2.0-or-later), same snapshot: the 2x2 Bayer dithering expression and the 6-bit output of the RGBA6 pixel format, in the pixel shader of `engine/src/gpu/gx_render.cpp`.

### GX logic operations from the Dolphin Emulator

- Repository files: `engine/src/gpu/gx_state.cpp` (`blend_state`) and the pixel shader in `engine/src/gpu/gx_render.cpp`, own code.
- Behaviour studied in the Dolphin Emulator's `Source/Core/VideoCommon/RenderState.cpp` (`BlendingState::Generate`, `ApproximateLogicOpWithBlending`, `LogicOpApproximationIsExact`, `LogicOpApproximationWantsShaderHelp`) and `Source/Core/VideoCommon/BPMemory.h` (`BlendMode`, `LogicOp`), current `master` read on 2026-09-26 ("Copyright 2017 Dolphin Emulator Project" and related years, GPL-2.0-or-later). No code was copied: the priority rules (blend enable first, with subtract over the blend factors; logic operation only when blending is off; no-op keeps only a constant alpha write), the register layout and the idea of letting the shader output 0, 255 or the inverted color are reimplemented. The blending table for the inexact operations was derived again for this project and differs from Dolphin's.

### Line and point texture offsets from the Dolphin Emulator

- Repository files: `engine/src/gpu/gx_state.cpp` (`line_point_offsets`, `line_offset_negative_side`) and `emit_line` / `emit_point` in `engine/src/gpu/gx.cpp`, own code.
- Behaviour studied in the Dolphin Emulator's `Source/Core/VideoBackends/Software/Clipper.cpp` (`CopyLineVertex`, `ProcessLine`, `CopyPointVertex`), `Source/Core/VideoCommon/GeometryShaderManager.cpp`, `GeometryShaderGen.cpp` and `BPMemory.h` (`LPSize`, `TCInfo`), current `master` read on 2026-09-26 ("Copyright 2009 Dolphin Emulator Project" and related years, GPL-2.0-or-later). No code was copied: the register bits, the offset table (0, 1/16, 1/8, 1/4, 1/2, 1, 1, 1) and which expanded vertices receive the offset are reimplemented.

### DSP core and free DSP ROMs from the Dolphin Emulator

- Repository files: everything under `third_party/dolphin/`, kept in Dolphin's own directory layout so it stays separate from this project's code. `third_party/dolphin/COPYING` and `third_party/dolphin/LICENSES/GPL-2.0-or-later.txt` are copied from the same snapshot.
- Upstream files (2606 snapshot, exact commit unknown): `Source/Core/Core/DSP/DSPCore.*`, `DSPTables.*`, `DSPAccelerator.*`, `DSPHWInterface.cpp`, `DSPMemoryMap.cpp`, `DSPStacks.cpp`, `DSPAnalyzer.*`, `DSPHost.h`, `DSPCommon.h`, `DSPBreakpoints.h`, `DSPCaptureLogger.h`, all of `Source/Core/Core/DSP/Interpreter/`, and `Source/Core/Common/CommonTypes.h`, `BitField.h`, `Inline.h`.
- Copyright, as stated in each file header: "Copyright 2008/2009/2010/2014/2018 Dolphin Emulator Project", and in some files also "Copyright 2004 Duddie & Tratax" or "Copyright 2005 Duddie". Every header carries `SPDX-License-Identifier: GPL-2.0-or-later`, compatible with GPL-3.0-or-later.
- `third_party/dolphin/Data/Sys/GC/dsp_rom.bin` and `dsp_coef.bin`: Dolphin's free replacements for the DSP instruction ROM and coefficient ROM (version 0.4, written by Dolphin contributors: duddie, LM, ligfx, Tilka, Pokechu22 and others; history and source in `docs/DSP/free_dsp_rom/` of the snapshot). They contain no Nintendo code. They carry no per-file notice, so they fall under the repository licence, GPL-2.0-or-later (`COPYING` of the snapshot). They are embedded in the executable as resources 2 and 3 (`games/wiiparty/res/wiiparty.rc`).
- Changes (2026-09-23, this project), all in `third_party/dolphin/`: the DSP JIT was removed from `DSPCore.cpp`/`DSPCore.h` (only the interpreter is used); `DSPInterpreter.cpp` reads time through a new `DSP::Host::TimeBase()` (declared in `DSPHost.h`) instead of Dolphin's `Core::System` timers; `DSPTables.cpp` formats one hex string with `snprintf` instead of `fmt`; `DSPAccelerator.cpp` includes `Common/MathUtil.h`; the `fmt::formatter` specialisations and the `fmt` include were removed from `BitField.h`. `Interpreter::HandleLoop` was moved from the private to the public part of `DSPInterpreter.h` so the recompiled microcode can call it. (2026-09-24) `SDSP::DoDMA` in `DSPHWInterface.cpp` ignores a DMA longer than 0x4000 bytes instead of calling `std::exit(0)`, and `Interpreter::GetMultiplyProduct` in `DSPInterpreter.cpp` computes the unsigned product as `static_cast<u32>(a) * b` instead of `static_cast<u32>(a * b)`, which multiplied two `int`s with signed overflow (undefined behaviour; GCC at `-O2` produced a sign-extended product). Both only change inputs the game's microcode does not produce.
- Not copied: the DSP JIT, assembler, disassembler, capture logger implementation, symbols, and Dolphin's `Core::System`, `CoreTiming`, `Memmap` and logging. `third_party/dolphin_shim/` holds small replacements written for this project (no-op logging and assertions, a mutex-based `Common::Event`, page allocation with `calloc`, an Adler-32 hash, `MathUtil::SaturatingCast` and a `PointerWrap` that serialises into a byte vector, used by `WP_DSP_VERIFY`), so the Dolphin files compile unchanged otherwise.
- `engine/src/audio/dsp.cpp` and `engine/include/wp/dsp.h` (Dolphin copyright line and SPDX identifier): the CPU side of the DSP adapted from Dolphin's `Source/Core/Core/HW/DSP.cpp` (DSP interface registers, control register, ARAM DMA, audio DMA and its interrupts, "Copyright 2008 Dolphin Emulator Project"), `HW/DSPLLE/DSPLLE.cpp` (mailbox and control register glue, the DSP thread, "Copyright 2008") and `HW/DSPLLE/DSPHost.cpp` (the `DSP::Host` functions, "Copyright 2009"), all GPL-2.0-or-later. Rewritten for this project's memory and interrupt model: registers are reached through the `0xCC005xxx` check in `engine/include/wp/memory.h`, interrupts are delivered by `engine/src/core/interrupts.cpp`, the DSP runs in lockstep on the CPU thread (as in Dolphin's single-core LLE mode, without its thread), and audio DMA blocks go to `engine/src/audio/audio.cpp` (own code, WASAPI output).

- `recompiler/dsp/tables.py` (own code) reads the opcode templates and interpreter handler tables from `third_party/dolphin/Source/Core/Core/DSP/DSPTables.cpp` and `Interpreter/DSPIntTables.cpp`, and the idle-skip signatures from `DSPAnalyzer.cpp`, at build time; nothing from those files is copied into the tool. The C++ generated from the microcode uses the helpers below, calls the Dolphin interpreter for the few opcodes that are not inline, and is not committed (it is derived from the game's microcode).
- `engine/include/wp/dsp_inline.h` and `recompiler/dsp/inline_ops.py` (Dolphin copyright lines and SPDX identifier, GPL-2.0-or-later): inline versions of the DSP interpreter, adapted from Dolphin's `Source/Core/Core/DSP/Interpreter/DSPInterpreter.cpp` (register access, address register arithmetic, accumulator and product helpers, status register updates and condition codes), `DSPIntCCUtil.h`, `DSPIntUtil.h`, `DSPIntArithmetic.cpp`, `DSPIntMultiplier.cpp`, `DSPIntLoadStore.cpp`, `DSPIntMisc.cpp`, `DSPIntBranch.cpp` (branches and `HandleLoop`) and `DSPIntExtOps.cpp` ("Copyright 2008 Dolphin Emulator Project, Copyright 2004 Duddie & Tratax" in `DSPInterpreter.cpp`, "Copyright 2005 Duddie" in `DSPIntUtil.h`, "Copyright 2009 Dolphin Emulator Project" in the others; all four lines are kept in both files). The header holds the helpers as `always_inline` functions over the public `SDSP` state; the Python module emits, for each supported opcode, the statements of the matching interpreter handler with the operand fields already decoded. Written for this project: the status register updates are computed without branches and, where the next instructions overwrite them before anything reads them, only when the translated code is left (same result, checked by `WP_DSP_VERIFY`).

## Libraries linked but not included in the repository

### SDL3

- Used by `engine/src/input/gamepad.cpp` (own code) for gamepads: buttons, sticks, gyroscope and accelerometer. The build links it only when it is found; `engine/src/input/no_gamepad.cpp` is used otherwise.
- Version 3.4.16, the official MinGW development package `SDL3-devel-3.4.16-mingw.tar.gz` from https://github.com/libsdl-org/SDL/releases, downloaded by `tools/fetch_sdl.py` into `build/deps/` (ignored by Git) and checked by SHA-256. `SDL3.dll` is copied next to the executable and must be shipped with it.
- Copyright: "Copyright (C) 1997-2026 Sam Lantinga", as stated in `LICENSE.txt` of the package. License: zlib, which is compatible with GPL-3.0-or-later. A binary distribution should carry that `LICENSE.txt` (an acknowledgment is appreciated but not required). No SDL source is copied or changed.

### Qt 6 (launcher)

- Used by `launcher/` (own code) through its public API: Qt Core, Gui and Widgets 6.11. Not included in the repository; on Windows the MSYS2 package is used and `launcher/cmake/deploy_windows.cmake` copies the DLLs (Qt and its dependencies: ICU, HarfBuzz, FreeType, zstd and others, each under its own license) next to the executable at build time.
- License: Qt is available under LGPL-3.0 and GPL-3.0 (among others). Used as dynamically linked libraries, which is compatible with this project's GPL-3.0-or-later. A binary distribution of the launcher must carry the licenses of Qt and of the DLLs shipped with it.

### Tools downloaded by the launcher

- On Windows the launcher downloads, only when missing, the official builds of nodtool 1.4.4 (GitHub encounter/nod), Python 3.12.10 embeddable (python.org), CMake 4.4.3 (GitHub Kitware), Ninja 1.13.2 (GitHub ninja-build) and the nuwen.net MinGW distribution 20.0 (GCC 15.2), checking the SHA-256 recorded in `launcher/src/toolchain.cpp` (CMake's matches its published checksum file). They are stored in the user's `build/deps/toolchain/`, never in the repository; their licenses apply to their use.



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

Ghidra, Dolphin, nodtool, dtk, CMake, Ninja, GCC (MinGW-w64), Python and its packages listed in `tools/requirements.txt` (including resvg-py, which renders the icon at build-tool time) are used to build or analyse the project. None is included in the repository. Their licenses apply to their own use.

## Items in this repository pending review

- `games/wiiparty/analysis/dol_functions.csv`: function addresses and sizes obtained by analysing the user's own `main.dol`. It contains no game code, but it is derived from game data.
- `games/wiiparty/analysis/symbols.csv` and `games/wiiparty/analysis/hle_functions.csv`: symbol names for SDK functions. Written during this project; part of the names were checked against a symbol map exported from Dolphin, which is kept local and not committed. Their origin has not been audited.
- `recompiler/lz11.py`, `recompiler/ppc/decoder.py` and other tools: written for this project. No known third-party code, but not audited.
- Code in this repository was written with the help of an AI assistant. Whether and how that affects copyright is a question for legal advice.

## Prohibited material

Not to be used, even though the project is GPL:

- Code from InputEvelution/wp without a valid authorization or license.
- Leaked, proprietary, unauthorised or unknown-origin code.
- Original Wii Party data: `main.dol`, `.rel` files, ISO or parts of it, textures, models, sounds, fonts, videos and any other protected resource.

`games/wiiparty/disc/`, `games/wiiparty/extracted/` and `reference/` stay ignored by Git.
