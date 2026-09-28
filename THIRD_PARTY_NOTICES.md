# Third-Party Notices and Provenance Tracking

The original code of this project is licensed under the **GNU General Public License, version 3 or later (GPL-3.0-or-later)**. 

> [!WARNING]
> **Asset Protection & Intellectual Property Boundaries:** The project's license does not apply to, nor does it grant any rights over, Nintendo's proprietary game data or binaries.

---

## Current Status
This document establishes a rigorous provenance tracking system for all external components adapted or integrated into this repository. It serves as a continuous engineering log of reviewed components and pending audits.

---

## Ingested Third-Party Components (GPL-2.0-or-later)

All components adapted from the **Dolphin Emulator** originate from a source snapshot of **version 2606** (stored locally under the untracked path `reference/dolphin-source/`). 

### 1. GX Lighting Pipeline
- **Repository Files:** `engine/src/gpu/gx_lighting.cpp` | `engine/include/wp/gx_lighting.h`.
- **Upstream Origin:** `Source/Core/VideoBackends/Software/TransformUnit.cpp` (Register mapping references `VideoCommon/XFMemory.h`).
- **Copyright:** Copyright 2009 Dolphin Emulator Project.
- **License Compliance:** `SPDX-License-Identifier: GPL-2.0-or-later` (Fully compatible with GPL-3.0-or-later).
- **Engineering Changes (2026-09-22):** 
  - Rewritten to read from this project's flat XF register array instead of packed structs.
  - Shifted internal operations to standalone RGBA bytes instead of ABGR layouts.
  - Stripped out upstream panic alert handlers and fixed divide-by-zero edge cases on null normal lengths.

### 2. Emulated Bluetooth Controller
- **Repository Files:** `engine/src/input/bluetooth.cpp` | `engine/include/wp/bluetooth.h`.
- **Upstream Origin:** `Source/Core/Core/IOS/USB/Bluetooth/BTEmu.cpp`, `IOS/USB/USBV0.cpp` (USB V0 layout), and `BTBase.h` (buffer constants).
- **Copyright:** Copyright 2008 Dolphin Emulator Project.
- **License Compliance:** `SPDX-License-Identifier: GPL-2.0-or-later`.
- **IP Isolation Note:** Downstream files `hci.h` and `l2cap.h` were **not** copied due to licensing mixtures (FreeBSD/NetBSD BSD-licensed code). All HCI opcodes and event structures were clean-room written from the official Bluetooth specification.
- **Engineering Changes (2026-09-23):** 
  - Adapted connection mechanics and L2CAP signaling from `WiimoteDevice.cpp`.
  - Refactored around this runtime's asynchronous IOS request architecture (`ios::take_completion`).
  - Replaced packed structs with native byte vectors and eliminated save-state overhead.

### 3. Emulated Wii Remote State Machine
- **Repository Files:** `engine/src/input/wiimote.cpp` | `engine/include/wp/wiimote.h`.
- **Upstream Origin:** `Source/Core/Core/HW/WiimoteEmu/EmuSubroutines.cpp`, `WiimoteEmu.cpp`, and `Camera.cpp`.
- **Copyright:** Copyright 2010/2019 Dolphin Emulator Project.
- **License Compliance:** `SPDX-License-Identifier: GPL-2.0-or-later`.
- **IP Isolation Note:** Upstream motion simulation (`Dynamics.cpp`), extensions, MotionPlus, speaker decoders, and cryptography layers were completely omitted.
- **Engineering Changes (2026-09-23):** 
  - Implemented a clean-room custom mapping math from PC pointer to dual IR coordinates, calibrated directly against native KPAD reports.
  - Linked reporting events back into the Bluetooth interface via lightweight callbacks.

### 4. DSP Hardware Core & Free Interpreter ROMs
- **Repository Files:** Full tree under `third_party/dolphin/` (Maintains upstream structural isolation).
- **Upstream Origin:** `Source/Core/Core/DSP/` core infrastructure, interpreter modules, and standalone data blobs.
- **Copyright:** Copyright 2004-2018 Dolphin Emulator Project / Duddie & Tratax.
- **License Compliance:** `SPDX-License-Identifier: GPL-2.0-or-later`.
- **Ingested Assets:** Includes `dsp_rom.bin` and `dsp_coef.bin` (Version 0.4 free alternative ROMs created by Dolphin contributors). Embedded as clean native C++ resources into `games/wiiparty/res/wiiparty.rc`. Contains zero Nintendo code.
- **Engineering Changes:** 
  - Completely purged the upstream DSP JIT engine; forced exclusive lockstep execution of the interpreter directly on the main CPU thread.
  - Patched signed integer overflows (undefined behavior at `-O2` optimization flags) in `GetMultiplyProduct` inside `DSPInterpreter.cpp`.
  - Bound compilation dependencies through a dedicated local shim layer (`third_party/dolphin_shim/`), preventing external library leakage.

---

## Externally Linked Libraries (Permissive Licenses)

These dependencies are dynamically linked during runtime or retrieved through automated build scripts; no code is hosted directly inside the repository.

### SDL3 Development Package
- **Usage Area:** Hardware gamepad mappings (Sticks, gyroscope, accelerometer processing) inside `engine/src/input/gamepad.cpp`.
- **Version:** 3.4.16 (Retrieved by `tools/fetch_sdl.py` into build tree).
- **License:** **zlib License** (Fully compatible with GPL-3.0-or-later).
- **Copyright:** Copyright (C) 1997-2026 Sam Lantinga.

### Launcher Libraries (`launcher/`)
The launcher is a single static executable. CMake downloads each library below at configure time and checks its published SHA-256; none of them is stored in this repository. The launcher's Licenses page shows every license text.
- **SDL 3.4.16:** window, input, file dialogs and rendering, linked statically. **zlib License**. Copyright (C) 1997-2026 Sam Lantinga.
- **Dear ImGui 1.92.9b:** user interface. **MIT License**. Copyright (c) 2014-2026 Omar Cornut.
- **Inter 4.1 (Regular and SemiBold):** embedded font. **SIL Open Font License 1.1**. Copyright (c) 2016 The Inter Project Authors.
- **mingw-w64 winpthreads:** threading runtime of the MinGW-w64 GCC toolchain, linked statically. **MIT-style license** (`launcher/licenses/winpthreads.txt`). Copyright (c) 2011 mingw-w64 project.
- **GCC runtime libraries (libstdc++, libgcc):** linked statically. **GPL-3.0 with the GCC Runtime Library Exception 3.1** (`launcher/licenses/gcc-runtime-library-exception.txt`).

---

## Ingestion Pipeline Requirements
Before any external file or code snippet is introduced into this repository, the following five criteria must be strictly fulfilled and appended to this registry:

1. **Granular Path Mapping:** Specify the exact files reused, their upstream repository paths, and the precise commit hash or version snapshot.
2. **Copyright Maintenance:** Retain all original author credits and copyright lines entirely untouched within the source file headers.
3. **Licensing Isolation:** Record the exact license governing each specific file.
4. **Dependency Auditing:** Perform a deep-dependency check ensuring zero licensing incompatibilities exist with GPL-3.0-or-later.
5. **Structural Isolation:** Place all ingested code within dedicated directories, keeping them architecturally apart from the core project code.

> [!IMPORTANT]
> If a file's provenance, author origin, or license conditions are ambiguous or unverified, **the code must not be copied**.

---

## Prohibited Material
The following materials are strictly prohibited from entering this repository under any circumstances:
- Code originating from `InputEvelution/wp` without signed, verifiable owner authorization.
- Development leaks, proprietary platform SDK components, or unverified source code.
- Original Wii Party game resources (`main.dol`, `.rel` modules, ISO dumps, textures, models, audio).

*Note: The local paths `games/wiiparty/disc/`, `games/wiiparty/extracted/`, and `reference/` must remain permanently enforced in `.gitignore` [3].*
