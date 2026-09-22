# Third-party notices and provenance

The original code of this project is licensed under GPL-3.0-or-later (see `LICENSE`). The GPL does not apply to, and does not grant rights over, Nintendo's game data.

## Status

One adapted third-party component is included (see below). This file records what was reviewed and what is still pending. It is not a legal audit.

## Third-party code included in the repository

### GX lighting from the Dolphin Emulator software renderer

- Repository files: `src/gx_lighting.cpp` and `include/wp/gx_lighting.h`. They are kept apart from the project's own code and carry the upstream copyright line and SPDX identifier.
- Upstream file: `Source/Core/VideoBackends/Software/TransformUnit.cpp` of the Dolphin Emulator, from a source snapshot of version 2606 (taken from `CMake/ScmRevGen.cmake` in the local copy under `reference/dolphin-source`; the snapshot has no Git metadata, so the exact commit is not known).
- Parts adapted: `TransformNormal`, `SafeDivide`, `CalculateLightAttn`, `LightColor`, `LightAlpha` and the channel logic of `TransformColor`. The register layout was read from `Source/Core/VideoCommon/XFMemory.h` (same snapshot); no code from that file was copied.
- Copyright: "Copyright 2009 Dolphin Emulator Project", as stated in the upstream file header.
- License: `SPDX-License-Identifier: GPL-2.0-or-later`, as stated in the upstream file header. Dolphin's `COPYING` and `LICENSES/GPL-2.0-or-later.txt` confirm it. GPL-2.0-or-later code may be used under GPL-3.0-or-later, so it is compatible with this project.
- Dependencies: none copied. Dolphin's `Common::Vec3`, logging, assertion and memory types were replaced by small local equivalents; no other Dolphin file is included.
- Changes (2026-09-22, this project): reads lights and channel registers from this project's flat XF register array instead of Dolphin's `XFMemory` struct; works on RGBA bytes instead of Dolphin's ABGR layout; removes the panic alerts; returns the normal instead of dividing by zero when its length is zero; exposes two functions (`transform_normal`, `light_channels`) used by `src/gx.cpp`.

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
