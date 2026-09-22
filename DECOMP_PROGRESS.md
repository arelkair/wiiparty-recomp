# Decomp progress

## State

- Toolchain: CMake 4.4, Ninja, GCC 15.2 (MinGW-w64), Rust/Cargo, Python 3.12, JDK 25, nodtool 1.4.4. No PPC cross-compiler (not needed: this is recompilation, not matching)
- Ghidra 12.1.3 in `ghidra/install/` (ignored by Git)
- Disc: `game/wiiparty.rvz` extracted to `extracted/` (ID SUPP01, PAL)
- Binary: DOL parsed (`tools/dol.py`); 115 RELs decompressed to `build/rel/` with `tools/lz11.py` (all valid, REL v3 format, parser in `tools/rel.py`)
- Ghidra: DOL loaded with `ghidra/scripts/LoadDol.java` and the `PowerPC:BE:32:Gekko_Broadway` processor from the GameCube Loader; `tools/ghidra_import.py` imports, analyses, applies `analysis/symbols.csv` and exports the function list
- Ghidra MCP: 6.0.0 bridge installed in `.venv/`, extension in `%APPDATA%\ghidra\ghidra_12.1.3_PUBLIC\Extensions\GhidraMCP` (built for 12.1.2, version patched to 12.1.3); still to be enabled in the CodeBrowser
- Functions identified by Ghidra: 6115 (`analysis/dol_functions.csv`), 94.8% of the code covered (75.4% with the generic processor); `FindFunctions.java` adds 812
- Functions implemented: 0
- Lifter: `tools/recomp.py` generates C++ for 7014 DOL functions (473,789 instructions, 20 files in `build/recomp/`) and compiles with no errors or warnings under GCC
- Build: CMake + Ninja (`build/out`), libraries `wp_runtime` and `wp_dol`
- Tests: 3 suites in CTest (Python translator, C++ runtime, a lifted DOL function against a reference written from the Ghidra decompiler)
- Next objective: translate the remaining REL modules (`python tools/recomp_rel.py --all`), the GX layer (graphics), DSP/audio and input

## Target architecture

- CPU: PowerPC 750CL (Broadway), 32-bit, big-endian, with paired singles
- ABI: PowerPC EABI (Wii/GameCube SDK); r1 stack, r2/r13 small data areas
- Approach: DOL/REL lifter to C++ plus a runtime that reimplements the RVL SDK (OS, VI, GX, PAD/WPAD, DVD)

## Binaries

- `sys/main.dol`: 2,294,304 bytes, entry `0x80004050`, RVL SDK from 2009-2010
- Text: `0x80004000` (0x2720) and `0x800070e0` (0x1d5f20)
- Data: `0x80006720`, `0x80006bc0`, `0x801dd000`, `0x801dd0c0`, `0x801dd0e0`, `0x80207f80`, `0x802f5440`, `0x802f6900`
- BSS: `0x80231980`, size `0xc6e88`
- `files/rel/*.rel.lz`: 115 modules compressed with LZ11 (header `0x11` + size). They include `boot`, `menu`, `loading`, `openmess`, `ranking`, `inst`, `mg1xx`-`mg5xx` (minigames), `mr*` (boards and modes) and `ms*`
- No symbol map on the disc

## Execution (Milestone 1)

- `build/out/wiiparty.exe extracted [seconds]` loads the DOL, prepares low memory and runs `entry`; if it stops making progress it prints the call stack
- With the real boot data (`src/boot.cpp`, checked against Dolphin RAM dumps in `reference/ram-dumps/`), `OSInit` follows the true path, checks the disc drive (`0x8015bb60`) and finishes. The game reaches `main`, passes several video retraces, creates threads, reads data from the disc, initializes GX, VI, LYT, G3D, EF and RFL, loads and links `boot.rel`, runs its `_prolog`, reads `locale/en_EU/boot/strap.arc.lz` and enters the scene task loop (`0x80069ee0`), without blocking
- With incomplete boot data it reached `main` (`0x800070e0`) and waited for the vertical retrace in `VIWaitForRetrace` (`0x80147860`)
- Strategy: the SDK hardware is not simulated; functions are replaced by runtime handlers (`analysis/hle_functions.csv` lists the names, `analysis/symbols.csv` resolves them to addresses, `src/hle.cpp` implements them)
- Replaced so far: `EXIInit/Lock/Unlock/Probe/GetID` (EXI bus without devices), `__OSInitAudioSystem` (DSP), `OSRealModeCall` (BAT), `IOSSendRequest` (all IPC with IOS, simulated in `src/ios.cpp`: devices `/dev/stm`, `/dev/fs`, `/dev/es`, `/dev/di` and NAND files)
- `sc` is a no-op (it only flushes caches in this SDK)
- `/dev/di` device (in `src/ios.cpp`): answers as a normal drive. Inquiry (0x12) returns zeros, unencrypted read (0x8d) outside the disc gives error 0x52100, ReportKey (0xa4) gives error 0x53100 and RequestError (0xe0) returns the last error; the other commands return success
- Interrupts: `WP_POLL` on every backward jump, every 16384 iterations, calls `poll_interrupts` (`src/interrupts.cpp`); if the game has EE enabled and the retrace is due (fixed 20 ms period for now), it sets the flags at `0xCC002030/34` and calls the handler in the table at `0x80003040 + 4*24` with the current context in `r4`, then `OSSelectThread` (`0x8013fad0`)
- Threads (`src/threads.cpp`): each game thread is a Windows fiber. Calls to `OSSaveContext` (`0x80138290`) are translated with `setjmp` (`wp_jump`) and `OSLoadContext` (`0x80138310`, replaced) returns to that point with `longjmp` or starts a new fiber if the context came from `OSInitContext`; registers are saved in the game's own `OSContext`. `OSSwitchFiber` (`0x80138400`) changes `r1` and calls the function. Windows only for now
- Controllers: `WPADInit` and `KPADInit` (`0x8017b5a0`, `0x80193cf0`) are replaced by empty versions (no controllers connected). Planned design: a neutral layer holding the state of one controller and pluggable backends (keyboard and mouse first; generic gamepads and real Wii Remotes later)
- `OSReport` (`0x80138880`) and `OSPanic` (`0x80138900`) are replaced and show the text formatted by the game (`src/format.cpp`)
- REL (`tools/recomp_rel.py NAME...` or `--all`, output in `build/rel_code/`): the game decompresses and links each module with its own `OSLink`. Each module is translated with synthetic addresses (`section << 24 | offset`); relocations are resolved at generation time: calls into the DOL are direct (`f_XXXXXXXX`), internal calls are direct (`f_<module>_<address>`), data addresses are `g_<module>_bases[section] + addend`, and addresses in other modules use `wp::external_address`. `src/modules.cpp` identifies the loaded module by a signature (identifier, sections and bss size) and updates its bases at the first jump. The DOL targets requested by modules are stored in `build/rel_dol_targets.csv` and `recomp.py` adds them as entries
- `boot` module (identifier 1): 5 functions, 248 instructions; its 31 calls into the DOL, 18 own data addresses and 2 pointers in data resolve correctly
- Virtual NAND (`src/nand.cpp`, folder `game/nand`, ignored): default `SYSCONF` (English, 4:3) generated on mount, save and settings files with read, write and seek, and `/dev/fs` commands (create file and folder, delete, rename, attributes, list)
- Initial video state (`src/boot.cpp`): control register `0xCC002002 = 0x0101` (PAL, active) and interrupt registers `0xCC002030/34 = 0x1001`. If the display is not active `VIInit` programs NTSC and the game picks `/locale/en_US`
- Debugging: `WP_LOG_DISC=1` shows disc reads, `WP_DUMP=file` dumps memory on failure
- Pending: derive the retrace rate from the video mode the game configures, free the fibers of finished threads, and real disc data reads, OS threads, time, real DVD, NAND file reads (SYSCONF, `play_rec.dat`)

## Lifter

- `tools/ppc/decoder.py`: PowerPC/Gekko instruction decoder, including paired singles. It decodes 472,251 of 472,265 code words; the remaining 14 are embedded data
- `tools/ppc/emit.py`: translates each instruction to C++ on `wp::Cpu`
- `tools/ppc/cfg.py`: jump labels and jump tables (104 resolved)
- `include/wp/cpu.h`, `include/wp/memory.h`, `src/runtime.cpp`: CPU context, big-endian memory and instruction semantics
- Each function is `f_XXXXXXXX(wp::Cpu&)`; `bl` to a known function is a direct call, the rest go through `wp::call` (lookup in `g_function_table`)
- Unsupported instructions: only `rfi` (39 uses, exception handlers)

## Local tools

- `reference/dtk/dtk-windows-x86_64.exe` (decomp-toolkit 1.8.4, Apache-2.0): `dol info` and `rel info` confirm the DOL and REL format read by `tools/dol.py` and `tools/rel.py`
- `tools/merge_rels.py`: merges the DOL with one module into `build/elf/<module>.elf` (all of them with `--all`, 87 s), to open them in Ghidra with relocations resolved. A single ELF with the 106 modules is not feasible: dtk grows disproportionately (10 modules 5 s, 20 modules 29 s, 40 modules over 100 s)
- 8 groups of modules share an internal identifier (`mg103`/`mg418`, `mg110`/`mg411`, `mg111`/`mg417`, `mg210`/`mg504`, `mg215`/`mg408`, `mg216`/`mg506`, `mg507`/`mg508`/`mg509`, `ms601`/`ms602`), so they cannot be loaded at the same time
- `tools/ghidra_decompile.py 0xADDRESS ...`: decompiles functions without opening Ghidra (needs the project closed)

## Structures, symbols and offsets

- Real low memory (from our own dump): FST at the end of MEM1 (`0x81800000` minus the size of `fst.bin`), BI2 just below, MEM2 ends at `0x93600000` with the IOS heap in the last `0x20000`, interrupt handler table at `0x80003000`, PAL video mode = 1

- Symbols: `analysis/symbols.csv` holds the names set by hand (versioned); `reference/symbols/SUPP01.map` is the map exported from Dolphin with its signature database (`Sys/totaldb.dsy`), local and not versioned, and `tools/import_map.py` converts it to `build/dolphin_symbols.csv` (2544 real names, 5280 unnamed entries). The 22 manual names match the map in every case where the map has a name. The runtime call stack prints the names
- IOS IPC request: `+0x00` command, `+0x04` result, `+0x08` descriptor, `+0x0C` arguments, `+0x20` callback, `+0x24` callback argument; commands 1 open, 2 close, 6 ioctl, 7 ioctlv
- Pointer to the IPC heap: `r13-0x78e4`; free a request: `0x80177300`
- Video retrace: counter at `r13-0x713c`, thread queue at `r13-0x7160`

## Hypotheses and known problems

- Lifter: the OE (overflow) bit is ignored; `fres`/`frsqrte` use `1/x` instead of the Broadway estimate table; `blrl` and indirect calls depend on `g_function_table`
- Lifter: 172 `bctr` without a detected jump table; 158 are virtual calls (`lwz`+`mtctr`+`bctr`), treated as an indirect jump and return; 14 remain to be reviewed
- Lifter: besides `dol_functions.csv`, it discovers entries from `bl` targets, external jumps, pointers in data, `lis`+`addi` address constants and table targets; a function that falls into the next one is linked with a final call
- Lifter: function bounds = next entry of `dol_functions.csv`; no runtime yet (uninitialized memory, hardware, `sc`, exceptions)

- Ghidra 12.1.3 does not ship the Gekko/Broadway processor; the one from the GameCube Loader is used (installed in `%APPDATA%\ghidra\ghidra_12.1.3_PUBLIC\Extensions\`, source in `ghidra/extensions/`)
- Names of very small functions in the Dolphin map can be wrong (for example `OSGetCurrentContext` on several different getters), because the signature database gives the same name to identical bodies
- Ghidra has no EABI ABI; the `default` (System V PPC 32) is used
- The header BSS overlaps `.data5`-`.data7`; `LoadDol.java` only creates BSS blocks in the gaps

- The game logic lives mostly in the RELs: the recompiler has to support dynamic loading and REL relocations, not only the DOL
- The DOL holds the SDK and the common engine; the RELs are loaded per scene

## Plan

1. Review the coverage of the DOL analysis
2. Import the RELs into Ghidra with relocations
3. Identify SDK functions and the symbol table in `analysis/`
4. PPC instruction lifter and CMake build system
5. Extend the lifter to the RELs

## State after translating all modules

- The 115 REL modules are translated (`python tools/recomp_rel.py --all`, about 650 MB of C++) and the full executable compiles (about 316 MB). Recompiling everything takes on the order of 15 minutes with 12 threads.
- Diagnostic profiler: `WP_PROFILE=1` samples the innermost function every millisecond and prints it when the timer expires; with `WP_DUMP=file` it also dumps the guest memory.
- The game's task engine (0x80069af0-0x8006a96c) uses `setjmp` (0x801c8a1c) and `longjmp` (0x801c8b20) as coroutines. The lifter emits a native `setjmp` at calls to `setjmp` and the runtime replaces `longjmp` with a fiber switch; a buffer whose link or stack differs from the saved one is treated as a new context and starts a fiber at the saved address. Modules must be regenerated to include this change.
- The locked cache (0xE0000000-0xE0003FFF) has its own region in the memory buffer. It used to overlap low memory and `LCEnable` erased the system globals, including the interrupt table.
- `GXDrawDone` (0x8014fe50) is replaced: with no GPU, it finishes immediately by setting the flag it was waiting for.
- The retrace period is derived from the video configuration register (0xCC002002): PAL 20 ms, otherwise 16.683 ms.
- Current state: the game runs frames stably at about 50 per second, with the main thread and the frame thread, and loads the boot data. Without graphics output it is not yet possible to tell which screen it is on; the list of loaded modules is empty at that point.
- Audit of `main.dol`: the 14 illegal words are data inside the code area (the string "Metrowerks" and constant tables next to 0x80006680), the 169 indirect jumps without a table are jumps through a function pointer or virtual table and are resolved at run time with `wp::call`, and the 37 `rfi` are in the exception vectors and in functions already replaced or reachable only by real exceptions. No reachable code is left to translate in `main.dol`.
- Video output: `src/video.cpp` opens a Win32 window and, on each retrace, converts the external framebuffer (YCbCr 4:2:2, address and width taken from the VI registers) to RGB. The framebuffer stays empty because the GPU is not emulated yet; the game writes to the GX FIFO (0xCC008000) with ordinary instructions, so the next step is to capture those writes.
- Decoder fix: in D-form instructions (`ori`, `xori`, `addi`...) the last bit belongs to the immediate and is not the record bit. It was read as the record bit and overwrote cr0. Now only opcodes 4, 20, 21, 23, 30, 31, 59 and 63 accept the record bit. Checked with a decoder test and with a test of function 0x80039510 (red-black tree node erase) against libstdc++ `_Rb_tree_insert_and_rebalance` in `tests/lifted_tests.cpp`.
- GX FIFO capture: writes to 0xCC008000 (including those from `psq_st`) accumulate in `src/gx.cpp` and are decoded command by command (CP, XF, BP registers, display lists and draws); with `WP_LOG_GX=1` a summary is printed for each EFB copy to the framebuffer.
- Per-thread diagnostics: each fiber has its own call trace; the timer prints the stack of all fibers, and `WP_WATCH=address` reports changes of one memory word.
- The GX FIFO decoder now interprets the game's stream with no errors (0 unknown commands). Both floats of a `psq_st` pair go to the FIFO. `WP_LOG_GX=2` lists the draws (vertices, VCD and VAT) and `3` the first commands. Each frame in the current state contains three quads and one EFB copy to the framebuffer.
- GX renderer on Direct3D 11 (`src/gx_render.cpp`, state and vertices in `src/gx.cpp`): vertex decoding by VCD/VAT with direct or indexed attributes, transformation by the XF matrices (projection, viewport, texture coordinate generation), ubershader with the TEV stages, texture decoding (I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, CMPR), blending, depth, scissor and EFB copy to the external framebuffer converting to YCbCr 4:2:2. The first game screen (the strap notice) is drawn correctly. `WP_SAVE_FRAME=file.png` saves the presented image every 100 frames.
- Verified details: the game's cull mode 1 is equivalent to discarding faces with counter-clockwise winding on screen; the four masked BP registers (0xFE) apply to the next register; the TEV KONST constants share addresses 0xE0-0xE7 with the color registers and are told apart by bit 23.
- GX pending: lighting, indirect textures, lines and points, near-plane clipping, internal scaling, EFB copies in intensity formats.
- Pending: graphics (GX), audio (DSP), input (WPAD/KPAD), freeing the fibers of finished threads and accounting for the trace depth after a `longjmp`.
- Input with a real Wiimote: Windows asks for a PIN when pairing through the normal interface (with 1+2 and with SYNC). Pairing will be done inside the program with the Windows Bluetooth API, using the 6 bytes of the host Bluetooth address as the PIN, and reading the HID reports afterwards. Generic Pro Controller type gamepad: SDL2. User's monitor: ultrawide 2K at 75 Hz.

## User goals

- Output at 1080p with the game at 60 Hz. The PAL game picks 50 or 60 Hz from the `IPL.E60` setting in SYSCONF; it will be enabled from the program's settings.
- Internal resolution multiplied by 3 or 4, inside the project's own renderer.
- Rates above 60 Hz (for example 120 Hz) by interpolating object matrices between logic steps; experimental and after the renderer. The user's monitor is 75 Hz, which is not a multiple of 60.
- Online play without a server, with a fourth green "Online" button in the row of three lower buttons of the minigame selection. It needs step-based input synchronization, a shared random seed and clock, exchange of Mii data and a direct connection. The button will be added in memory when the menu module is loaded, without modifying the game's resources.
- Quality-of-life improvements: borderless full screen with F11, 16:9 widescreen and ultrawide, skipping notices and logos, controller mapping, frame counter and a configuration file.
- Distribution: the program will not include the game; it will use the extracted files of the user's copy, with a minimal launcher to choose the folder or the disc.

## Reference Dolphin

- Dolphin 2606-374 ships a GDB server: with `GDBPort = 2159` under `[General]` in `Dolphin.ini` it listens on port 2159 and accepts a single client per boot. `tools/dolphin_gdb.py` implements the GDB protocol client (registers, memory, breakpoints and watchpoints, steps) and with `--launch` restarts Dolphin on each call. Checked: stop at the entry point 0x80004050, memory read and stop at a breakpoint at 0x80069ee0.
- Planned uses: dump Dolphin's memory at the Wiimote strap notice and compare states and registers with those of `wiiparty.exe` in the same functions.

## Verified state on 2026-09-21 (audit of the block after the strap screen)

Verified by running `wiiparty.exe` and saving captures with `WP_SAVE_FRAME=path_%02d.png` (one every 100 retraces):

- With no button pressed, the game leaves the strap screen on its own (the game's own timer), shows the title and enters the host presentation sequence.
- With A and B held (`WP_INPUT_BUTTONS=0C00`, active after 150 reads), it reaches the main menu (Party Games, Pair Games, House Party and the three lower buttons) and stays stable. Input through `KPADReadEx` reaches the game.
- Causes of the earlier block, all fixed and checked by running: (1) asynchronous IOS completions ran inside the request itself and the game's DVD failed with `freeDvdContext.inUse`; they are now queued and delivered as an IPC interrupt (`ipc_deliver`); (2) audio calibration waited for the AI sample counter (0xCD006C08), now emulated in `src/audio.cpp`; (3) DSP startup waited for a processor that is not emulated: the 22 public functions of the AX core (0x8015ea40-0x80160b10) are replaced by versions that return zero (`HleZero_*`).
- Controller reading: `KPADReadEx` (0x801934d0, 0xF0-byte structure) and `WPADProbe` (0x8017c9e0) are replaced; only channel 0 is connected. Keyboard and mouse mapping in `src/input.cpp`. Generic gamepads and the real Wiimote are pending.

State by component:

- Implemented and tested: keyboard and mouse reading as a Wiimote, deferred IPC delivery, AI sample counter, rendering of the main menu with most elements.
- Stub: AX audio core (no sound) and DSP.
- Partial: GX. Missing lighting, indirect textures, lines/points and EFB copies in intensity formats.
- Pending: real audio (AX mixer on the PC side), generic gamepads and Wiimote, internal resolution, freeing the fibers of finished threads (dozens of `OSExitThread` fibers appear unfreed).
- Not verified: that the four KPAD channels give the state the minigames expect; that the title displays correctly at 16:9.

Build cost: each addition of a replaced function regenerates `functions.h` and recompiles all modules (about 15 minutes). Separating that declaration from the modules is pending.

## Palette textures and EFB copies (2026-09-21)

- Implemented in `src/gx.cpp` and `src/gx_render.cpp`: TLUT loading (BP 0x64/0x65, 512 KB palette TMEM copied at load time), formats C4, C8 and C14X2 with IA8, RGB565 or RGB5A3 palette (BP register 0x98/0xB8 per map), and EFB copies to texture (BP 0x52 without the XFB bit) for RGB565, RGB5A3 and RGBA8 through `CopySubresourceRegion` on the GPU.
- The copied texture is associated with its destination address and used only if the size matches and a 32-word sample of the guest memory has not changed (if the game writes another texture at that address, it is discarded).
- Verified by running with A+B: the main menu shows the animated minigame thumbnails and there are no more magenta squares or `unsupported texture format` warning. ctest 3/3.
- Limitations: the copy with the half-size reduction bit uses the full-size texture (sampled with normalized UVs); RGB565 does not force alpha to 1; intensity formats (R4, Y8, RA4, RA8, A8, R8...) are not copied (`unsupported EFB copy format` is logged).
- Still visible: a white box over "Pair Games", the dark House Party panel and the black menu background. Cause not investigated.

## Display lists and closing the intermittent failure (2026-09-21)

- Cause: `GXBeginDisplayList` redirects the write pipe (0xCC008000) to a memory buffer by changing the PI registers (base 0xCC00300C, write pointer 0xCC003014) while the CP registers (0xCC000020/22) still point to the main FIFO. The runtime sent those bytes to the GP instead of to memory, so the commands executed at the time they were recorded and the saved list held garbage. When it was called later (command 0x40) the parser read absurd addresses and lengths: intermittent segfault (4 of 6 runs) and missing text.
- Fix (`record_display_list` in `src/gx.cpp`): if the PI base differs from the CP base, the bytes are written to memory at the PI pointer and it advances. Also, a list call out of range is discarded with the warning `GX display list out of range`.
- Verified: 6 consecutive runs of 25 s with no failures (before, 4 of 6 with a segfault), ctest 3/3, and the main menu now shows the text (Suggestions, Rankings, Minigames, 1 to 4 players, About 45 min.).
- New diagnostic: `WP_LOG_FROM=N` with `WP_LOG_GX=2` starts the draw log after copy number N; texture maps and vertex arrays are shown.
- Still pending: black menu background (the 3D background draws come out with invalid vertices, cause not investigated; the badly recorded lists may have been the reason, to be rechecked) and partial House Party panel.

## Recheck after the display lists (2026-09-21)

- With `WP_LOG_FROM=1200` the background draws no longer come out with invalid vertices: they were a consequence of the badly recorded lists.
- With no input, the game goes through the logo animation, shows the title ("Wii Party" logo at the correct size and framing, "Press A and B together", "(c)2010 Nintendo") and reaches the main menu on its own (captures `t_05` to `t_08` of `WP_SAVE_FRAME`). The wrong title framing noted earlier does not reproduce.
- Not verified against Dolphin: whether the black menu background is the original.
- Visible defect (fixed on 2026-09-21): the House Party preview must show 4 Miis and the title must show the pink background with 3D characters; both are skeletal 3D models and are not drawn correctly.

## Skeletal 3D models: diagnosis (2026-09-21)

- Dolphin reference provided by the user: the title has a pink/white background with 3D Miis and animals, the menu has a pink gradient background, red/orange/green panels and purple/blue/yellow buttons, and House Party shows 4 Miis. Our render: black background, no 3D models and different panel colors (blue/purple/green, turquoise/lime/purple buttons).
- Evidence: the model draws (commands 0x90/0x98/0xA0 with a position matrix per vertex) arrive with correct raw positions, but the XF matrices they use (indices 3, 6, 15, 27...) are zero. They are loaded with indexed loads (commands 0x20 and 0x30) from arrays of 48 bytes per matrix (different bases per model in MEM1), and the contents of those arrays are zero also when the FIFO is processed at the moment it is written (a flush threshold of 64 bytes was tried and reverted). So it is not a problem of delayed parsing.
- Ruled out: `PSMTXConcat` (0x8014b100) is compared with a reference matrix multiplication with 50 random cases and matches (test added in `tests/lifted_tests.cpp`); the constant `(0,1)` it uses is at 0x802f58d0.
- Pending: find which code should write those arrays (computation of the models' world matrices) and why it does not, or stores zeros; hypothesis: a function replaced by HLE, a thread/coroutine that never runs, or skeleton data not loaded.
- Not investigated yet: menu color difference and performance (the user perceives 30-40 fps with drops).

## STM eventhook, log noise and fps measurement (2026-09-21)

- An asynchronous `IOSSendRequest` on `/dev/stm/eventhook` completed instantly, so the SDK resent it endlessly (hundreds of log lines and CPU time). That request now stays pending forever (`ios::never_completes`), as in real IOS until there is a reset or power-off event. Warnings about unhandled ioctls are logged once per device and command.
- New diagnostic `WP_LOG_FPS=1`: prints once per second the EFB to XFB copies (real game frames). Measurement with the main menu: 50 fps at the start, dropping steadily to about 20 fps in 30 s, so there is a degradation over time (suspects: unfreed finished fibers, texture cache, per-draw cost). Not investigated yet.
- `/shared2/menu/FaceLib/RFL_DB.dat` does not exist in the virtual NAND: the Mii database is empty (not verified whether this affects the missing Miis).

## Target platforms and the decision on aurora (2026-09-21)

- User's goal for final versions: Windows 10 and 11 (32 and 64 bits respectively, x86 and ARM), Linux and macOS.
- Current state, all Windows-specific: GDI window (`src/video.cpp`), Direct3D 11 with HLSL (`src/gx_render.cpp`), threads as Windows fibers (`src/threads.cpp`), build with GCC from MinGW-w64.
- Consequences: Linux and macOS need a cross-platform graphics backend (WebGPU with Dawn or similar), a portable window and input layer (SDL3) and portable coroutines. Windows 11 does not exist in 32 bits; a 32-bit executable with about 320 MB of guest memory and about 300 MB of code is the riskiest target and is left for the end.
- Decision: aurora (MIT) is not adopted whole for now. The Direct3D 11 renderer is kept to get a working game, and `include/wp/gx_render.h` is kept as a narrow boundary (draw, copy, clear, load palette) so a WebGPU backend can be added later. Pieces of aurora without graphics dependencies (texture conversion) can be incorporated under its MIT license when they help.

## Locked cache DMA: cause of the missing 3D models (2026-09-21)

- Cause: the game moves matrices and other data with locked cache DMA (`LCLoadBlocks` at 0x80137c40 and `LCStoreBlocks` at 0x80137c70, writes to SPR registers 922/923 DMA_U and DMA_L). The runtime stored the write without doing the copy, so the skeleton matrix arrays stayed at zero. In Dolphin's RAM dump those arrays held real matrices (evidence that the game does compute them).
- Fix: `wp::locked_cache_dma` (`src/runtime.cpp`) performs the copy when DMA_L has the trigger bit (memory to cache with the load bit, cache to memory without it; length in blocks of 32 bytes, 0 means 128) and clears the trigger and flush bits. The emitter (`tools/ppc/emit.py`) calls it after `mtspr 923`.
- Tool: `tools/recomp.py` no longer rewrites generated files that have not changed nor deletes existing ones, so a change to the emitter only recompiles the affected files.
- Verified: with the main menu the 3D background and the four House Party Miis are now visible (captures `d_04`, `d_05`). ctest 3/3.
- New visible problems: the image looks washed out (too light, with a white veil) and fps drop to about 5 in 3D scenes (50 fps on 2D screens). Not investigated.

## Menu colors and veil (2026-09-21)

- The white veil seen after fixing the DMA was a transition fade; it is not a fixed defect.
- Added the TEV channel swap tables (BP 0xF6-0xFD, per-stage selection in bits 0-3 of each stage's alpha register) to the shader; they were ignored before. No visible effect in the menu, but needed for intensity textures and other materials.
- The system clock (`time_base`) now starts from the real date since 2000-01-01 instead of zero. Hypothesis ruled out: the menu colors do not depend on the date.
- Diagnostics: `WP_LOG_GX=2` prints per draw the TEV color registers (`regs=`), the konst, the order, the material color and the channel control. With it, for the "Party Games" panel the game writes the value (0, 140, 255, 255), blue, to color register 1; in Dolphin the panel is red. The difference is therefore in the data the game decides, not in the renderer's conversion (which only decodes what it receives).
- Unresolved: which game input chooses the color (candidates: missing `/wiiparty.bin` save data, missing `RFL_DB.dat` Mii database, SYSCONF settings, a floating-point or quantized animation computation lifted wrongly).

## Renderer performance (2026-09-21)

- Measurement: with `WP_PROFILE=1` almost all the time was in `GXDrawDone` (GX processing on the host). The game emits about 5,600 draws per frame in 3D scenes (particles and Miis) and each cost about 30 microseconds of Direct3D 11 overhead; result: 6 fps in 3D and a drop from 50 to 20 fps in the menu.
- Changes in `src/gx_render.cpp`: consecutive draws with the same state (TEV constants, textures, samplers, blending, depth and scissor) accumulate and are sent with a single call; the batch is flushed before any EFB copy, clear or texture replacement. Textures are only queried for the maps the TEV stages use, and a texture already checked in the current frame is not hashed again.
- Result measured with `WP_LOG_FPS=1`: stable 50 fps (the PAL cap) in title, menu and 3D scenes for 70 s (before, 6 fps in 3D).
- Pixelation: the image is rendered at 640x480 (native resolution) and enlarged by GDI without filtering. The planned fix is scaled internal resolution (2x, 3x, 4x) and GPU presentation; pending.

## Goals and tools suggested by the user (2026-09-21)

- Extra goal: a Galician translation of the game (text only, without distributing game resources).
- Real visual reference provided by the user (Dolphin, PAL in Spanish): the title has a pink background with 3D Miis and animals; in the main menu the five Party Games boards are red, the pair games are orange-yellow and House Party is green, with the Miis somewhat smaller than in our version. The lower buttons are purple, blue and yellow.
- WinDbg with Time Travel Debugging: useful to record a failure of the executable and step backwards; limitations: the executable is built with GCC (DWARF symbols that WinDbg reads poorly), the trace of a CPU-intensive emulator is huge and slows execution a lot. Reserved for hard-to-reproduce failures.
- RenderDoc: capture of a Direct3D 11 frame with its calls, textures, buffers and shaders. It fits our renderer directly and is the planned tool for the pixelation, colors and textures.

## Scaled internal resolution and GPU presentation (2026-09-21)

- Cause of the pixelation: the EFB was rendered at 640x480 and the window enlarged it with GDI. Now the EFB has `WP_SCALE` times the native resolution (range 1 to 6; default 1, native resolution, since 2026-09-21): the viewport and the scissor are multiplied by the scale and EFB-to-texture copies keep the high resolution.
- The EFB to XFB copy is no longer read back to memory as YUV or converted on the CPU: a GPU copy leaves the frame in a texture (`copy_to_framebuffer`) and `present_frame` draws it on a DXGI swap chain (flip, no vertical sync) with linear filtering, centered and with black bars according to the aspect ratio (16:9 with `IPL.AR=1`). The GDI painting and the YUV code in `src/video.cpp` have been removed; `WP_SAVE_FRAME` now saves the frame at internal resolution through `read_frame`.
- Verified by running: sharp title and menu (1920x1440 captures), 45 to 51 copies per second with `WP_LOG_FPS=1` in the 3D menu, ctest 3/3.
- Limitations: the guest XFB memory is no longer filled (the game does not read it; not verified in other modes); no vsync (Present(0, 0)); texture mipmaps are not generated; the textures of the Mii faces show blocks and their filtering has not been investigated.

## Menu colors: ruled-out hypotheses and findings (2026-09-21)

- Ruled out by measurement: time and date (12 runs with the clock shifted give pixel-identical captures) and a channel rotation in the TEV color and konst registers (tried and reverted: it breaks the Mii skin and does not change the panels, so the color of those panels does not come from those registers).
- Finding: in Dolphin's dump and in ours the player color table is at the same addresses (0x1de148: blue, dark blue, red, dark red, green, dark green, orange, dark orange, purple, dark purple, pink...), that is, blue, red, green and orange are the colors of players 1 to 4. There are also copies of those colors next to the names of the user's Miis inside player structures in Dolphin's dump (0x6f5bb4 with "Arel Kair").
- Dolphin's panels are red, orange and green; ours are colors from another table (turquoise, lime) or rotated; still unexplained.
- With the user's NAND (Mii database `RFL_DB.dat` of 779,968 bytes copied to a temporary NAND, not versioned) the game hangs initializing RFL: it opens the file, seeks to 0, reads 127,456 bytes successfully (the asynchronous completion is delivered) and issues no more requests; the wait loop at 0x8009a4d0 stays in state 6 (busy). To be investigated; fixing it would allow using the real Miis.
- New diagnostics: `WP_LOG_IOS=1` prints each IOS request and each asynchronous completion with its callback.
- The window opens at 1920x1080 by default (reduced to fit the monitor's work area).

## Decrementer and OS alarms; Mii database (2026-09-21)

- Cause of the RFL hang with the user's Mii database: after the read, the next stage is scheduled with an operating system alarm (`OSSetPeriodicAlarm`, about 19 ms) and the runtime did not emulate the decrementer (DEC) or its exceptions, so no alarm ever fired. A race condition was ruled out earlier (a minimum latency of 1 ms was added to asynchronous IPC completions, which is more realistic, but it was not the cause).
- Fix: `mtspr 22` and `mfspr 22` are emitted as `wp::set_decrementer` and `wp::get_decrementer` (`tools/ppc/emit.py`, `src/runtime.cpp`), with the DEC tied to the time base clock. `poll_interrupts` delivers the decrementer exception (index 8 of the exception table at 0x80003000) with `save_context` and a resume point: the SDK handler saves the context, processes the alarms and ends with `OSLoadContext`, which resumes the interrupted thread or switches to another as on hardware. Leaving the interrupt through a fiber switch clears the interrupt flag.
- Diagnostics: `WP_LOG_IRQ=1` prints each decrementer exception; `WP_LOG_IOS=1` prints IOS requests and completions.
- Verified: with the user's Mii database (`RFL_DB.dat`, in a temporary unversioned NAND) the game finishes the RFL initialization (opens, seeks, reads and closes the file), shows the Miis from that database in the title, stays at 50 fps and reaches the Party Phil presentation with the correct pink colors of his clothes. With the default NAND it still reaches the menu. ctest 3/3.
- General effect: any game or SDK code that depends on alarms (DVD timers, `OSSleepTicks`, start buttons, RFL) now works; before it failed silently.
- Not verified: the colors of the main menu panels with the real Miis, and the impact of the DEC on other screens.

## Input and shutdown diagnostics (2026-09-21)

- `WP_LOG_INPUT=1` prints the focused window and each button change the game reads. It confirmed that keyboard and focus worked (the input failure the user noticed came from an old instance of the window).
- Crash reporter (`src/main.cpp`): an exception filter prints the exception code, the host address, the access address and the guest call stacks (current thread and the rest of the threads) before terminating. It was added because a process exit when entering a game on the first board left no message.

## First real failure while playing (2026-09-21)

- The user entered the first board (Board Game Island) and reached the Mii selection. The crash reporter showed `exception c0000005 ... reading address` (read outside the memory reserved for the guest) after confirming the Mii, with normal game threads and task queue. Probable cause: texture, palette or vertex array addresses outside the 320 MB reserved (a 24-bit address shifted by 5 can reach 512 MB, and the guest memory covers 320 MB). Not verified by running; the user has to repeat the test.
- Changes: `render::guest_range_valid` checks ranges before reading textures, palettes and vertex arrays (arrays out of range read zeros); the crash reporter now also prints the offset relative to the module to locate the function with `addr2line`; `WP_LOG_INPUT` only prints when the focus changes; `WP_LOG_FPS` adds batches, vertices and CPU-side draw time per second to investigate the performance drop on this screen.
- Observation: the printed call stack shows repetitions of `80136f30 -> 80136d00 -> OSLoadContext` (decrementer exceptions resumed with `longjmp`); the trace is not rewound by `longjmp`, so it is diagnostic noise and not a real recursion.

## Draw submission performance and locating the memcpy failure (2026-09-21)

- User's measurement with `WP_LOG_FPS`: on the Mii selection screen fps drop to 15 to 23, with 5,000 to 9,800 batches per second and between 550 and 830 ms of each second spent in `flush_pending` (draw submission, CPU side), about 110 microseconds per batch.
- Probable cause: each batch did `Map` with `DISCARD` on a vertex buffer of about 7 MB (65,536 vertices of 108 bytes), forcing the driver to rename the whole buffer hundreds of times per frame. Change: a vertex ring with `NO_OVERWRITE` and a cursor (`DISCARD` only when full), and `Draw` with an offset. Not measured yet after the change.
- Failure when starting the game: the module offset `0x1d613` corresponds to `f_800043c4` (+3473), the game's own `memcpy`, reading a source address outside the 320 MB of guest memory. The origin of the pointer is earlier; the crash reporter now prints the guest registers (`lr`, `sp`, `r3` to `r7`, `r30`, `r31`), and `lr` indicates the caller.
- The call trace depth is restored after resuming a decrementer exception, so the printed stack no longer shows false repetitions of `80136f30`.

## memcpy failure when starting Board Game Island (2026-09-21)

- Measurement after the vertex ring: draw submission time went from 550 to 830 ms per second to 5 to 20 ms, with stable 50 fps also on the Mii selection. Resolved and verified by the user.
- The failure when pressing Start repeats in the same function: `f_800043c4` (the game's `memcpy`) reading outside the 320 MB. Registers at the failure: `lr=0100f344`, `r3=807054e0`, `r4=40000008`, `r5=c9998d78` (garbage length of about 3.4 billion), stack `800043c4 <- 0100f0f0 <- 01002980 <- 01002020 <- 01000410 <- 010001a0 <- 8002b580 <- 80028d90 <- main`. The `0100xxxx` addresses are functions of a dynamically loaded REL module, called right after a function of the main executable (`8002b580`) links a module; it fits the initialization of the game scene.
- Main hypothesis: a length or pointer computed from module data with a wrongly resolved relocation, or data the game reads from a resource that did not load. Not verified.
- The crash reporter now also prints the loaded modules, with their name and the section and offset that contain `lr`, to identify the exact function.

## Board Game Island loads: module translator and pointer failures (2026-09-21)

- Scripted input (`WP_INPUT_SCRIPT`, in milliseconds since the first controller read): entries separated by `;` with `from,to,buttons_hex[,x,y]` (pointer position from -1 to 1 over the image). It allows automating tests of full paths (the game uses the script instead of the keyboard and mouse).
- Pointer failure: the game uses the vertical pointer position without the 0.75 factor that `kpad_read` applied; with it, everything at the bottom of the screen (OK, Start, Minigames) could not be reached properly. `kPointerHeightScale` becomes 1.0. Verified: with the pointer over "Minigames", "Minigames" is now highlighted (before, House Party was highlighted).
- Path reproduced: title, menu, Board Game Island, number of players, Mii choice, CPU difficulty (that screen is the game's own), Start game.
- Cause of the failure when starting the game: the module translator (`tools/recomp_rel.py`) splits each module into functions at the entry addresses, including those that appear from relocations (C++ exception tables), but when a chunk ended it did not continue into the next one. Functions such as the constructor `0x0100d5d0` of `mr001` were cut in half, with no `return`, and returned a garbage pointer; then a vector `push_back` called `memcpy` with a garbage length and went out of memory.
- Fix: if the last instruction of a chunk can fall through (it is not an unconditional jump or return), a call to the next chunk is emitted. The 115 modules were regenerated. `tools/recomp_rel.py` no longer rewrites files that do not change.
- Verified by running: after the change the same path reaches the 3D island, the four Miis at the entrance and Party Phil ("Since this is your first time playing, I'll explain how this game works."), with no failures. ctest 3/3.
- Not verified: the rest of the modules (minigames and other boards) after the general fix; the `unsupported EFB copy format 8` warning (EFB copy in R8 format) is still pending.

## Aspect ratio and default resolution (2026-09-21)

- The image looked stretched: the window presented at 16:9 (from `IPL.AR` in the SYSCONF) what the game draws at 4:3 without squeezing. Checked with two NANDs that differ only in `IPL.AR` (0 and 1): the menu frame is identical in layout, so the game does not react to that flag as it runs now. Presentation is now always 4:3 and the initial window is 1280x960 (it shrinks keeping 4:3).
- Unresolved: check whether the real game uses anamorphic 16:9 in Dolphin with the same SYSCONF (it may read the aspect another way). `nand::widescreen()` is kept for that investigation.
- Default internal resolution: native (`WP_SCALE=1`); `WP_SCALE` 2 to 6 multiplies.
- Failure at the end of the board explanation: `0x80042140` calls a virtual function of an NW4R layout (`0x8012aa70`) with a null pointer returned when the layout is built after loading `layout/inst/cont012.arc.lz` and `cont013.arc.lz`. The cause of the black boxes in the explanation is not known: the `.mv` files (`inst/thumbnail/mg408.mv`, `mg410.mv`) are read at the end, on the minigame instruction screens, not during the explanation, so the earlier statement that the boxes are `.mv` videos is not supported. `missing_function` now prints the registers and the vtable of the object.

## Goal: ultrawide displays without bars

- Goal: the game fills 21:9 and 32:9 windows without black bars.
- Plan: internal image with the window's aspect ratio; horizontal field of view widened by modifying the projection of 3D scenes; in 2D layouts anchor each element to an edge or to the center and extend the backgrounds.
- Order: after fixing colors, flat Miis and textures; not implemented yet.
- Licenses: Dolphin and the RecompCore fork are GPL v2 or later, not MIT; they are used only as a behavior reference, without copying code.

## GPL-3.0-or-later license (2026-09-21)

- The project adopts GPL-3.0-or-later as the license of its own code. `LICENSE` holds the official GPLv3 text; `README.md` and `CLAUDE.md` are updated.
- New `THIRD_PARTY_NOTICES.md` with the rules for incorporation, the projects reviewed as possible sources (Dolphin, WiiCompiled, Aurora, RecompCore) and the project's own items pending review.
- Reuse of external code depends on an individual review of license and provenance. No external code has been incorporated and no project code has been modified in this task.
- This is not a complete legal audit.

## Relocations in immediate compare and carry instructions (2026-09-21)

- Symptom: the game crashed with a call to address 0 (or a write outside memory) after the Board Game Island explanation, when the instruction module (`inst`) asked an NW4R layout accessor for a resource. The name of the resource arrived empty or as garbage (`0x80700000` instead of a pointer to a string).
- Cause: the module translator applied relocations only to the instructions that used `simm_expr` or `uimm_expr`. `addic`, `addic.`, `subfic`, `mulli`, `cmpi` and `cmpli` used the raw immediate. The compiler emits `lis rX, sym@ha` followed by `addic. rY, rX, sym@l` to load an address, so the low half of the address was lost and the pointer was truncated.
- Fix: those six instructions in `tools/ppc/emit.py` now go through `simm_expr` and `uimm_expr`, which apply relocations in modules and give the same result in the DOL. The 115 modules were regenerated (`python tools/recomp_rel.py --all`). New test `test_low_relocation_in_addic_record` in `tests/test_ppc.py`; it fails without the fix.
- Verified by running the scripted path (title, menu, Board Game Island, players, Mii, CPU skill, Start, then A presses) for 280 s with no crash. The game shows the explanation, the "Maze Daze" instruction screen (Rules, Controls, Practice, Start), starts the minigame, shows the play order ("Megan, Hiroshi, Luca, Guest A") and reaches the board turn screen. ctest 3/3.
- Still wrong: the explanation pictures are black rectangles; the minigame itself is drawn upside down or black; `unsupported EFB copy format 8` (R8 EFB copy) is still logged; the menu panel colors are unresolved (see the color sections above).
- Not verified: other modules and minigames; the crash earlier seen in `memset` after the same scenes did not reappear in this run, but it was not investigated separately.

## Freeze when the first Mii moves on the board (2026-09-21, in progress)

- Reproduced with the scripted path plus periodic A presses (about 250 s in). The main fiber stays inside `0x80126ad0`, a rotation-matrix builder from three Euler angles that reduces each angle by repeated subtraction. It never finishes when an angle is huge.
- The caller is `0x8005ade0`, which reads the rotation of an object at `+0xb8`, `+0xbc` and `+0xc0` (vtable `0x80400f4c`) and calls `0x80126ad0`. The Y angle (`+0xbc`) follows `a' = -2a - k` each frame, so its magnitude doubles and alternates sign (for example -2150, 2150, -6451, 10752, -23654, 45158) until it reaches about 1e12. The X and Z angles converge normally (each step is about 0.95 times the previous one).
- This looks like a proportional controller with a gain of 3 per step on the Y angle, which diverges. The gain is probably scaled by a time step or another value that differs from real hardware. The source of that value has not been found.
- The rotation setters are `0x80053de0`, `0x80053df0` and `0x80053e40`. The next step is to log who calls them with a large Y angle. Object addresses change between runs, so a fixed memory watch does not work; log at the setters instead.
- Checked and ruled out: the semantics of `ps_sum0` and `ps_muls0` match the PowerPC manual and the Dolphin interpreter.
- Other findings from the same session, not yet investigated: the barrel in the first minigame does not appear, the dice have no pips, Miis look flat (GX lighting is not implemented, which fits), and round text such as "Round 1" is not drawn.

## Progress image (2026-09-21)

- `analysis/progress.csv` lists seven components with a percentage and the basis for it. `python tools/progress.py` averages them with equal weight and writes `docs/progress.svg`, which the README shows. The figure (47%) is an estimate; update the CSV when a component changes.

## Menu panel colors: the color table is correct, the item order is not (2026-09-22)

- Added `wp::module_name_at(address)` (`src/modules.cpp`, `include/wp/modules.h`): given a synthetic REL address (as stored in `c.lr` for a direct intra/inter-module call) it returns the name of the loaded module it belongs to if exactly one non-DOL module is loaded, `"dol"` for a real address, or `"?"` if zero or more than one module is loaded (ambiguous, since synthetic section/offset numbering is not unique across modules). It reuses the module list walk from `describe_loaded_modules`; it does not print anything.
- Diagnostic method: temporarily patched the two generated color-lookup functions (`f_8000ffd0`, `f_8000fff0` in `build/recomp/dol_000.cpp`, not committed) to log, on every call: a call-order counter, the caller (`c.lr`) and its module, `c.ctr`, the input `r3`/`r4`, the returned RGBA value, CR, XER and FPSCR. Recipe to reapply: insert a `static uint64_t` counter and snapshot `c.r[3]`, `c.r[4]`, `c.lr`, `c.ctr` right after `WP_ENTER`, then print them together with the final `c.r[3]` right before each function's `return;`.
- Confirmed the table lookup itself is correct: `0x801DE148 + id*8 + variant*4` reproduces every known RGBA entry exactly (id 1 variant 0 = `ff3838ff` red, id 3 = `ff9c00ff` orange, id 4 = `10bd0dff` green, etc.), matching the Dolphin RAM dump from an earlier session. Arguments and return value both check out for whatever `id` is requested; the bug is not in this function.
- Traced the call site: both `f_8000ffd0`/`f_8000fff0` are called directly (`bl`, not through a vtable) from about 1075 sites across almost every module; the ones seen during boot come from `f_menu_01012470` at `c.lr = 0x1012690u`, inside a loop over `r25 = 0..7` (one call per on-screen item). The `id` passed is not a compile-time constant: it is read from `*(uint32_t*)(*(struct_at_sp0x54) + r25*4)`, i.e. an array built earlier in the SAME function by a discovery loop (`f_menu_010133c0`, called up to 8 times, `L_01012530`) — the ids are discovered at run time, not stored as a fixed table in the REL's data section (checked: the on-disc bytes at the address the `lis`/high-relocation alone pointed to are an unrelated filename string, confirming the low half of that address is supplied per-access and the simple "static table" theory was wrong).
- Critical finding: running the same unmodified binary with no input three times in a row gives a DIFFERENT id order every time for that 8-item loop (`3,1,0,4,7,2,6,5` / `1,4,0,3,6,7,2,5` / `3,1,2,0,6,4,5,7`), before any player input and before the menu is shown. This happens on our own build, with no Dolphin comparison needed to prove it: the item-to-color assignment is not deterministic across our own runs.
- Leading hypothesis, not yet verified: whatever structure `f_menu_010133c0` fills (most likely a hash map/set keyed by a pointer or address, common in EGG/NW4R containers) has its iteration/insertion order depend on heap addresses that differ between runs. Real hardware would keep that order stable because its allocator only ever sees a fully deterministic sequence of earlier allocations; our decrementer and IPC completions are tied to real wall-clock time (`time_base()`, the 1 ms IPC latency), so a different number of ticks or thread switches can happen before this point in different runs, shifting later heap addresses. `g_memory` is zero-initialized with `calloc`, so stale host memory is not the cause.
- Not yet done: comparing this id sequence against Dolphin (now fast enough to use) with the GDB client, and testing whether making tick/interrupt timing deterministic (fixed virtual steps instead of real wall-clock) stabilizes the order across our own runs — that should be the first test, before touching Dolphin, since it is reproducible locally in seconds.
- No colors were forced, no palette entries were swapped, and no game code was changed in this session.

## Menu panel colors: solved, it is an intentional random shuffle (2026-09-22)

- Traced the array feeding `0x8000ffd0`/`0x8000fff0` (the color-id source for `f_menu_01012470`'s 8 on-screen items) at the two points that touch it: the fill loop and the read loop.
- The fill loop (`f_menu_010133c0`, a `std::vector`-style `push_back`) pushes exactly `0, 1, 2, 3, 4, 5, 6, 7` in that order, confirmed by logging the pushed value on each of the 8 calls (`PUSHBACK item_val=0..7`). No discovery, no hashing: this part is a trivial, always-identical fill.
- Between the fill and the read, the game calls `f_menu_01012960(begin, end, 0x8006b590)` on that array. Reading its body: it is a textbook in-place Fisher-Yates shuffle (loop from `count-1` down to `1`, at each step calling the third argument as `index = rng(i)` and swapping `array[i]` with `array[index]`).
- `0x8006b590` is a Park-Miller-style linear congruential generator with a global seed at `r13-0x8a38`. On its first call (seed == 0) it seeds itself with `OSGetTick() ^ 0xD8260000 ^ 0x0000BC89` (`0x80140b60` is `OSGetTick` per `build/dolphin_symbols.csv`); after that it updates the seed in place on every call.
- Conclusion: the game itself randomly shuffles which of the 8 palette colors (`0x801DE148`) lands on each menu item, seeded from the console's boot-time tick counter. This is not a bug in the table (`0x8000ffd0`/`fff0`, both proven correct earlier), not a bug in the fill loop (proven to always push `0..7`), and not something our own runs disagree on by accident: three consecutive runs producing three different permutations (`3,1,0,4,7,2,6,5` / `1,4,0,3,6,7,2,5` / `3,1,2,0,6,4,5,7`, all logged with `VECAT` before the shuffle result) is the *expected* behavior of a tick-seeded shuffle, since our `time_base()`-derived tick value at the moment of the first RNG call is not identical from run to run (nor would a real console's be, run to run).
- This reframes the original observation (title pink vs. blue, Party Games red vs. blue, Pair Games orange vs. purple, House Party green matching both) as most likely two independent draws from the same correctly-implemented random pool, not a translation bug. The open question is whether Dolphin's own boot path up to this RNG's first call is timing-deterministic enough that it always reproduces the same seed across its own reboots (plausible, since a cold Dolphin boot of the same disc is a much more controlled/repeatable sequence than reality) — if so, Dolphin would show a *stable* shuffle every time even though that stability is itself circumstantial, not evidence of a canonical/intended color.
- Suggested verification for the user (does not require our tooling): cold-boot the same Wii Party disc in Dolphin two or three times in a row and check whether the Party Games/Pair Games header colors change between boots. If they do, this closes the investigation entirely with no code change needed. If Dolphin is consistently the same across reboots, that is still not proof our colors are "wrong" — it would only mean Dolphin's specific timing up to the RNG's first call is more reproducible than a real console's, which is expected of an emulator.
- Diagnostic method (all temporary, not committed, build/ is gitignored): logged every call to `f_menu_010133c0` (vector push) and `f_menu_01013700` (vector element address, dereferenced) inside `build/rel_code/menu_000.cpp`, plus the existing `0x8000ffd0`/`fff0` trace in `build/recomp/dol_000.cpp` (see the "menu panel colors" entry above for the exact recipe). All instrumentation was reverted before this commit; `cmake --build build/out` and `ctest` were re-run clean afterwards.
- No colors were forced, no table entries were swapped, and no game code was changed. This closes the investigation unless the Dolphin reboot test above shows something unexpected.

## Menu panel colors: retracted the "solved" claim, renderer cleared, real cause still open (2026-09-22)

- The "intentional random shuffle" conclusion from earlier today was wrong: none of its 8 output colors match the actual on-screen panel pixels (sampled directly: Party Games band ~(227,228,243) pale blue-lavender, Pair Games band (163,12,255) vivid purple, House Party band ~(126,195,113)/(41,140,66) green). That shuffled array is not the source of the panel colors; it likely feeds something else (candidate: the decorative rings behind the bouncing Miis on the title screen, which do visibly vary between runs without that being wrong).
- Also ruled out, by a direct visual experiment: `f_menu_01044e60` (caller `0x10451ac`), which assigns ids 0,1,2,3 sequentially (deterministic, not shuffled) to 4 constructed objects. Temporarily forced its 4 outputs to magenta/cyan/yellow/black and re-ran: none of the three visible panels changed color at all, so this call site does not drive the panel backgrounds either. (Purely diagnostic; reverted immediately, not committed.)
- Checked whether the renderer/TEV pipeline itself could be at fault (the user's question). Re-verified today, with the current build, that the game's own GX FIFO commands still set TEV color register 1 to raw bytes decoding to RGBA(0,140,255,255) exactly as found two days ago; our BP register decode (the dual use of 0xE0-0xE7 for regular vs. konst colors, distinguished by bit 23) matches the documented hardware layout, and the HLSL TEV stage combiner (`color = (d + sign*lerp(a,b,c) + bias) * scale`) matches the real GX TEV formula. No bug found in the FIFO parser or the shader by inspection.
- Correlated actual GX color-register values to on-screen position by hooking `draw()` with a position+register logger, restricted to the exact moment the main menu is showing (a wall-clock window right after the strap/title sequence, not the whole run). This produced a clean, decisive result: the RGBA(200,63,45)/(170,90,0)/(90,200,90) colors (red/orange/green, matching the expected Dolphin colors and the static `foreColor` values already found in `mode_select.brlyt`'s `g_text_da`/`g_text_pc`/`text_f_02` materials) are being sent to GX right now, at the correct screen positions for the "Party Games"/"Pair Games"/"House Party" text labels. So the header **text** is already colored correctly.
- The wrongly-colored element is therefore the panel **background fill**, not the text on top of it. Those background draws (large quads/3D-model geometry behind the text) were not found among the register-colored draws; they carry `vc0=(255,255,255,255)` (white/neutral vertex color) and empty TEV registers, and some very large local-space coordinates consistent with the 3D background models noted on 2026-09-21 (`mg_bg.brres`, `pair_bg.brres`, `living_bg.brres`). This points at the background's color coming from a **texture** baked into those models, not from a register or a vertex color, which had not been checked yet.
- Next concrete lead, not yet investigated: decode the actual texture(s) used by `mg_bg.brres`/`pair_bg.brres`/`living_bg.brres` offline (format, palette/TLUT) and compare against what our texture cache produces, since a texture format/palette bug would explain a background-only color error while leaving text (which uses TEV registers, already shown correct) unaffected.
- All instrumentation used today (in `src/gx.cpp`, `src/gx_render.cpp`, `include/wp/gx_render.h`, and generated files under `build/`) was temporary and has been reverted; `cmake --build build/out` and `ctest` are clean.

## Menu panel colors: found the exact wrong register, root cause still open (2026-09-22)

- Decoded `mg_bg.brres`'s textures offline (Python, reusing the exact tiling/format logic from `decode_texture` in `src/gx_render.cpp`, verified against the file's own declared TEX0 size: header is 64 bytes, e.g. a 256x128 IA8 texture is exactly 64 + 256*128*2/... matches `65536 + 64 = 65600` bytes declared). The main background texture (256x128, format 3/IA8) is a soft white/pink glow gradient, not a flat color: it is meant to be tinted at runtime, not to carry the panel's color itself.
- Found the exact TEV stage used by both the Party Games header strip (`bbox=(-0.891,0.644)-(0.841,0.856)`, almost full screen width) and its body panel (`bbox=(-0.841,0.295)-(0.841,0.644)`): `ce=0008248f` decodes to `color = lerp(register1.rgb, register2.rgb, texture.rgb)`, i.e. the panel blends between register 1 and register 2 using the glow texture as the blend mask. `register2` is `(255,255,255,255)` (white) for both. `register1` — the tint color — is `(0,0,0,0)` (black) for both draws.
- The same register (TEV color register 1) is set correctly to `(200,63,45,0)` (red, matching the `g_text_da` material and Dolphin) for the separate draw that renders the "Party Games" text label, a few lines earlier in the same capture window. So the mechanism that sets register 1 works correctly for the text and fails (stays at zero/default) specifically for the background fill.
- This means the true bug is narrowly scoped: whatever game code is supposed to set TEV register 1 to red before the background 9-slice/glow draw either isn't running, is targeting the wrong pane/object, or is being skipped, leaving the register at its cleared value. It is not a texture decode bug, not a vertex color bug, not the earlier-considered PowerPC/table-lookup functions (`0x8000ffd0`/`fff0`), and not the TEV shader (its formula matches the GX spec and was re-verified against a fresh capture).
- Not yet found: which specific game function is supposed to write red into register 1 for this draw, or why it doesn't. The two calls investigated on 2026-09-22 earlier today (`f_menu_01012470`'s shuffled loop, `f_menu_01044e60`'s sequential loop) are both ruled out; the `nw4hbm_lyt_Pane_AnimateSelf`/`Animate` path found the same day is still the most likely mechanism (it was seen driving TEV register 1 for a different, small element), but the specific pane instance responsible for the header/body background has not been identified.
- Diagnostic method (temporary, reverted, not committed): hooked `render::draw()` to print, for every draw whose vertex bounding box falls inside a roughly normalized -1..1 screen area during a fixed 22-24 second window of a scripted run, all four TEV color registers, all four konst registers, vertex colors, and the active TEV stage words. Offline texture decode used a new (uncommitted) `tex0.py` in the session scratchpad; nothing was added to the repository's `tools/`.
- Build and tests verified clean after reverting all instrumentation (`cmake --build build/out`, `ctest` 3/3).

## Flat Miis: normals, per-vertex texture matrices and dual texture transform (2026-09-22)

- Cause of the flat look: Wii Party shades Miis (and other models) with environment/ramp textures sampled through texture coordinates generated from the vertex normal, not with GX color-channel lighting. The texgen path in `src/gx.cpp` had four gaps: the normal was read and discarded, `source == 1` (normal) fell through to a constant coordinate, per-vertex texture matrix indices were skipped, and the dual texture transform (XF `0x1009`, `0x1050+i`, post matrices at `0x500`) was not implemented.
- Data gathered with temporary logging in `generate_texture_coordinates` (reverted) on the scripted path to Board Game Island: the Mii environment material uses `info = 0x05086` (source normal, STQ, ABC1) with per-vertex texture matrices (`vmask` 01/03/06, indices 30-57) that hold each bone's normal rotation, dual transform on, and post matrix `[0.5 0 0 0.5][0 -0.5 0 0.5][0 0 0 1]` with the normalize bit (`0x100`). Signed 8-bit normals read with `/127` had length about 0.5, which showed the scale was wrong.
- Also found a pre-existing bit error: the code used bit 0 of `TexMtxInfo` as "projected" and bit 1 as the input form. Per the XF layout (checked in `reference/dolphin-source/Source/Core/VideoCommon/XFMemory.h`), bit 1 is the projection (ST/STQ) and bit 2 is the input form (AB11/ABC1).
- Implemented, following Dolphin's `VertexShaderGen.cpp`, `PixelShaderGen.cpp` and `VertexLoader_Normal.cpp`: normals decoded with the hardware fixed scale (u8 2^-7, s8 2^-6, u16 2^-15, s16 2^-14, float as is); raw (untransformed) normal as the texgen input, since the texture matrix carries the rotation; per-vertex texture matrix indices stored in `Vertex::texture_matrix`, defaulting to XF `0x1018`/`0x1019`; AB11 forces z to 1; for ST the third component is 1; dual transform normalizes when bit 8 of the post info is set and then applies the 3x4 post matrix at `0x500 + index*4`; the result is always divided by q, and q == 0 uses `clamp(xy/2, -1, 1)`. The division is per vertex, as before; Dolphin does it per pixel, so large projected triangles can still differ.
- The first attempt (the stash left by the previous session) rotated the normal by the position matrix and skipped the post matrix. It improved the look by chance. The real normal matrix at XF `0x400 + index*3` was checked with data and equals the 3x3 of the position matrix in the samples, but the hardware does not use it for texgen, so that path was dropped.
- Bisection with temporary environment switches (reverted): with the dual transform on but the old bit handling, the pink floor lines of the menu background vanished. They use the normal-based STQ material. With the correct bits and the Dolphin division they appear again.
- Verification: `cmake --build build/out`, `ctest --test-dir build/out` 3/3. Ran the scripted path (`WP_INPUT_SCRIPT` from `%TEMP%/wpcap/script.txt`, `build/out/wiiparty.exe extracted 115`, `WP_SAVE_FRAME`) and compared frames 10 (title), 26-45 (menus) and 53-57 (four Miis at the Board Game Island entrance) against the same run at `fe911a4`. Miis on the title and on the board now have light and shade. Menus and the rest of the path show no regression. Not checked against Dolphin frames; the face block artifacts were not checked at a higher resolution; no unit test was added because these functions are internal to `gx.cpp`.
- Intermittent crash found while testing, not caused by this change: `c0000005` with guest `lr=8000472c`, stack `fill_mem <- 0x80004714 <- 0x8017cd60 <- 0x80067410 <- menu module`. `0x8017cd60` (right after `WPADProbe`) reads the controller block from table `0x802b62d0[chan]` and queues HID report `0x15` (status request) into the queue at `block+0x164`. It is most likely `WPADGetInfoAsync`. `WPADInit` is replaced by an empty version, so the queue pointer is never set up. At `fe911a4` it did not show in 4 runs. With the new texgen code it shows in about half the runs, and it also shows with a variant that computes the normals but discards them (output identical to the baseline), so it is timing-dependent and not a rendering bug. Pending: replace `WPADGetInfoAsync` in HLE.

## GX color-channel lighting, adapted from Dolphin; corrections to the previous entry (2026-09-22)

- Correction to the previous entry: the dual texture transform enable is XF `0x1012` (`XFMEM_DUALTEX`), not `0x1009`, which is the number of color channels (`XFMEM_SETNUMCHAN`), checked in `reference/dolphin-source/Source/Core/VideoCommon/XFMemory.h`. Commit `144beb3` read `0x1009 & 1`, so the dual transform was on whenever one color channel was active. Fixed.
- Correction to the previous entry: the pink lines of the menu background are animated (they fade in and out). The claim that the dual transform made them vanish, and that the bit fix brought them back, came from comparing frames taken at different moments and is not reliable. Comparing runs whose frames line up exactly in time (same pink-pixel count per frame, fixed offset of 3 frames) shows the lines are the same with and without lighting.
- The previous session's conclusion that the game never enables hardware lighting was wrong: it tested the wrong bit. With the Dolphin `LitChannel` layout (bit 0 material source, bit 1 enable, bits 2-5 and 11-14 light mask, bit 6 ambient source, bits 7-8 diffuse function, bits 9-10 attenuation), temporary logging (reverted) shows lighting on for most 3D draws. Mii skin: `color0 = 0x706` (material register `f0f0dc`, ambient `a48974`, light 0, clamped diffuse, spot attenuation with `cos = (1,0,0)`, `dist = (1,0,0)` and a very distant position, which acts as a directional light). Mii clothes: `0x707` (vertex color as material). Channel 1 is often `0x202`/`0x20a` with a zero ambient.
- Implemented `src/gx_lighting.cpp` (and `include/wp/gx_lighting.h`), adapted from Dolphin's software renderer (`Source/Core/VideoBackends/Software/TransformUnit.cpp`, GPL-2.0-or-later). Provenance, license and changes are recorded in `THIRD_PARTY_NOTICES.md`. The normal is transformed by the normal matrix at XF `0x400 + (index & 31) * 3` and normalized. The lights are at `0x600 + n * 16`, and ambient and material colors are at `0x100A`-`0x100D`. The result uses the same integer rounding as Dolphin (`material * (light + (light >> 7)) >> 8`). `rasterize_colors` in `src/gx.cpp` now receives the eye-space position and calls it. When a vertex has no color, the material register is used as before.
- Verification: `cmake --build build/out`, `ctest --test-dir build/out` 3/3, 50 fps on the title screen (`WP_LOG_FPS`), and the scripted path to Board Game Island with `WP_SAVE_FRAME`. Mii faces, heads and arms now have a lit side and a shaded side on the title, the Mii picker icons and the four Miis at the board entrance. Menus show no regression in time-aligned frames. The title screen also shows a die with pips. Not compared against Dolphin frames.

## Intermittent WPAD crash: WPAD state was never initialized (2026-09-22)

- Cause: `KPADInit` (`0x80193cf0`) was replaced by an empty function. The real `KPADInit` is what calls `WPADInit`, and the game never calls `WPADInit` directly (only `0x80065cf0` calls `KPADInit`). So the WPAD state was never set up: the control-block table at `0x802b62d0` stayed at zero and the blocks held whatever was in memory. About 13 SDK functions (`0x8017b7d0`, `0x8017b8c0`, `0x8017c2b0`, `0x8017cd60`, `0x8017d0d0`, `0x8017d7e0`, `0x8017ee70`, `0x8017f040`, `0x8017f320`, `0x801803a0`, `0x80180640`, `0x801817b0`, `0x80182da0`) queue commands through `block+0x164`; the one seen crashing was `0x8017cd60`, which cleared a queue entry through a garbage pointer.
- Identified `0x8017cd60` as `WPADGetInfoAsync(chan, WPADInfo*, callback)` in real Dolphin with `tools/dolphin_gdb.py` (`GDBPort = 2159` added to the user's `Dolphin.ini` after a backup and restored afterwards, byte for byte). The game calls it with channel 0, info `0x80728784` and callback `0x80067950`. The callback later receives `(0, 0)` from inside WPAD (`lr = 0x8017cd34`), so it is asynchronous. The info buffer then holds `dpd 0, speaker 0, attach 1 (Dolphin's emulated Nunchuk), lowBat 0, nearempty 0, battery 4, led 1, protocol 0, firmware 0`. The callback only sets a "ready" byte (`object+0xfbc`) when the result is 0.
- Also read in Dolphin: table entries `0x802b7300 + n*0xbe0`; each block's queue pointer (`+0x164`) is `block+0x16c`, capacity (`+0x168`) 24, indices at `+0x160`/`+0x161`. The original `WPADInit` (disassembled from the DOL with `tools/ppc/decoder.py`) initializes Bluetooth (`0x8018d250`) and, if that succeeds, calls `0x8017b310`. That function fills the table from `0x802b62a0 + 0x30` with blocks at `0x802b62a0 + 0x1060`, and for each channel calls the block reset `0x8017afd0` (memory only; sets status `+0x900 = -1`, "no controller", and sets up the queue) and `OSCreateAlarm` (`0x8013f780`). It then writes a hardware register (`0xcd0000c0`), reads system settings and runs callbacks, which were not reproduced.
- Fix in `src/hle.cpp`. `WPADInit` and `KPADInit` now run the same per-channel loop as `0x8017b310`: set the 16 channel bytes at `+0x1040` to `0xff`, fill the table, clear `+0x8e8`, call the game's own `0x8017afd0` and `OSCreateAlarm`, and clear `+0xbae`. `WPADGetInfoAsync` is replaced (added to `analysis/symbols.csv` and `analysis/hle_functions.csv`). For a connected channel it fills `WPADInfo` (battery 4, LED `1 << chan`, no extension, since input is reported as a bare Wii Remote) and queues the callback with result 0 about 10 ms later, delivered with the IPC callbacks in interrupt context. For other channels it calls the callback at once with `-1` and returns `-1`, as the real function does on error.
- Verification: temporary logging (reverted) showed the four blocks set up with the same addresses as in Dolphin (queue `block+0x16c`, capacity 24, status `-1`) and `WPADGetInfoAsync` called twice per run with the callback delivered. `ctest` 3/3. The scripted path ran 8 times in a row with no crash, all reaching the Board Game Island entrance. Before the fix, about half the runs crashed with this build. The captured frames show the usual path and no new controller messages.
- The other WPAD functions still see "no controller" in their blocks (there is no Bluetooth), while `WPADProbe` and `KPADReadEx` report channel 0 as connected. Functions such as rumble (`0x8017cf80`) now return early instead of writing through garbage.

## Widescreen: the SYSCONF was malformed, so the game ran in 4:3 (2026-09-22)

- The image looked narrower than in Dolphin (whose config here forces 16:9, `AspectRatio = 1` in `GFX.ini`). The game does support widescreen. `0x80072810` calls `SCGetAspectRatio` (`0x80170420`, item 1, type byte), stores a widescreen flag at `r13 - 0x7568` (`0x802f5ed8`, `r13 = 0x802fd440`) and computes the aspect as a float at `0x802f5680`. A memory dump (`WP_DUMP`) showed flag 0 and aspect 1.333.
- Cause: `SCInit` read the whole 16 KB `SYSCONF` (seen with `WP_LOG_IOS`) but rejected it. The SC buffer at `0x802aa660` held only an empty `SCv0 ... SCed` with size 0, so every system setting fell back to its default. The file written by `src/nand.cpp` had one offset too few: the format (checked in `reference/dolphin-source/Source/Core/Core/SysConf.cpp`) has `count + 1` offsets, the last marking the end of the last item. `IPL.CB` was also written as a bool; it is a 32-bit long.
- Fix: the generator writes the end offset, and `IPL.CB` is a zero long as in Dolphin. `ensure_sysconf` regenerates the file when it is malformed (bad magic, footer or offset table), so the broken file already in `game/nand` is replaced on the next start. It is generated by this project, not game data. After the fix the SC size is `0x3ffa`, the flag is 1 and the aspect is 1.778.
- `video.cpp` now presents at 16:9 when `nand::widescreen()` is true and at 4:3 otherwise. The first window is 1280x720 or 1280x960. The earlier note "the game does not react to `IPL.AR`" (entry "Aspect ratio and default resolution") was a side effect of the rejected SYSCONF.
- Verification: `ctest` 3/3. On the scripted path, the title circles stay round, the Mii proportions are unchanged, the menu rearranges its layout for 16:9, and the board entrance shows more scenery at the sides. The internal frame is still 640x480 (anamorphic, as on hardware). With the SC settings now read, other system settings also apply: English, stereo, no progressive scan, no PAL60. Not compared side by side with a Dolphin frame.
