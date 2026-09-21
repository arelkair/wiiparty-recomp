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
