# Wii Party Recomp

Static recompilation of Wii Party for native PC.

> [!NOTE]
> **Project Status:** The game successfully boots, reaches menus, and runs complete game sessions. Progress is estimated at **79%** overall. For full details, see the referenced documentation.

![Project progress](docs/progress.svg)

---

## Overview

Wii Party Recomp translates original Wii executable binary code directly into native, statically-compiled C++ source code for high performance on PC without traditional software emulation layers.

---

## Current State & Metrics

The native executable runs stably on Windows at a locked **60 fps**, utilizing approximately 1.5 CPU threads and 200 MB of system RAM.

### Core Progress Metrics
Refer to `games/wiiparty/analysis/progress.csv` for tracked metrics:

| Component | Progress | Technical Foundation / Basis |
| :--- | :--- | :--- |
| **Recompilation Toolchain** | **95%** | DOL binary and all 115 REL modules translate and build. |
| **System Runtime** | **91%** | OS, fibers, threads, and hardware interfaces function without blockers. |
| **Graphics (GX to D3D11)** | **86%** | Menus, 3D lit Miis, and rendering pipelines. |
| **Input System** | **66%** | Emulated Wii Remote via virtual Bluetooth. |
| **Audio Pipeline** | **83%** | Recompiled DSP audio microcode. |
| **Game Flow** | **80%** | Full matches and minigames execute smoothly. |
| **PC Features** | **52%** | Qt launcher and options panels. |

---

## Feature Breakdown

- **Static Translation & Emulation:** Compiles main binaries into a single native program and emulates Revolution OS and Direct3D 11 rendering.
- **Limitations:** Minor block artifacts on high-resolution Mii renders and lack of support for physical Wii Remotes.

---

## Input Mappings & Controls

Mappings support keyboard, mouse, and gamepads, and can be customized in `settings.ini`.

---

## Repository Layout

```
├── engine/             # Core reusable Wii engine
├── recompiler/         # PowerPC & DSP static recompilation pipelines
├── games/wiiparty/     # Wii Party configurations and analysis dumps
├── launcher/           # Cross-platform Qt 6 installer and config manager
├── tools/              # Developer scripts
├── tests/              # Test suites
└── docs/               # Technical specs and progress markers
```

---


# Environment Setup & Installation

### Step-by-Step Build Pipeline

```bash
nodtool extract games/wiiparty/disc/wiiparty.rvz games/wiiparty/extracted
python recompiler/unpack_rels.py
python recompiler/recomp.py
cmake -S . -B build/out -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/out
```

Run the compiled executable:
```bash
./build/out/wiiparty
```

---

## Legal & Licensing

Independent project not affiliated with or endorsed by Nintendo. Users must supply their own legally obtained game files.
