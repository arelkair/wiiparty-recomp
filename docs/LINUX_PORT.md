# Linux port plan

State on 2026-09-29, measured with GCC 13.3, CMake 3.28 and Ninja 1.11 on Ubuntu 24.04 (WSL 2 on the development PC).

## What already works on Linux

- **Recompiler:** Python only; no Windows dependency.
- **Translated code:** a DOL chunk (`dol_003.cpp`) and a module chunk (`mg100_000.cpp`) compile unchanged with Linux GCC (`g++ -std=c++20 -O1 -Iengine/include`). The generated code only uses `wp/cpu.h`, `wp/hle.h`, `wp/threads.h` and the C++ standard library.
- **Engine sources that pass a Linux syntax check (24 of 36):** all of `core/` except `threads.cpp` (`boot`, `dol`, `format`, `fpu`, `hle`, `interrupts`, `log`, `modules`, `runtime`), `gx_lighting.cpp`, `gx_state.cpp`, `bluetooth.cpp`, `keymap.cpp`, `wiimote.cpp`, `disc.cpp`, `ios.cpp`, `ipc.cpp`, `options.cpp`, `save_backup.cpp`, `screenshot.cpp`, `settings.cpp`, `ui_text.cpp`, and the no-op variants. `gamepad.cpp` only needs SDL3 headers.
- **Launcher:** SDL3 and Dear ImGui; `subprocess.cpp` already has a POSIX branch (`fork`, process group, `setpriority`).

## What is Windows-only

| File | Lines | Windows use | Linux replacement |
| --- | --- | --- | --- |
| `gpu/gx_render.cpp` | 2,540 | Direct3D 11 renderer, HLSL compiled at run time | New backend on SDL_GPU (Vulkan on Linux, Metal on macOS, Direct3D 12 or Vulkan on Windows); shaders translated with SDL_shadercross or kept as SPIR-V built at compile time |
| `platform/video.cpp` | 589 | Win32 window on its own thread, DXGI swap chain, `SystemParametersInfo` | SDL3 window; presentation through the SDL_GPU swap chain |
| `audio/audio.cpp` | 307 | WASAPI output with windowed-sinc resampling | `SDL_AudioStream` (SDL3 resamples); keep the queue and timing logic |
| `input/input.cpp` | 342 | `GetAsyncKeyState`, `GetCursorPos`, focus through `GetForegroundWindow` | SDL3 keyboard, mouse and focus state (the key map already uses its own key names) |
| `platform/options_window.cpp` | 301 | F10 menu drawn with GDI into an overlay texture | Draw the same panel into a pixel buffer (already RGBA) and upload through the renderer |
| `core/threads.cpp` | 391 | Win32 fibers for guest threads, `OSSaveContext` and `setjmp` resumption | Small `wp::fiber` layer: Win32 fibers on Windows, `ucontext` (`makecontext`/`swapcontext`) on Linux and macOS |
| `gpu/custom_textures.cpp` | 272 | PNG decoding with WIC | `SDL_LoadPNG` (SDL 3.4) or a vendored decoder |
| `gpu/gx.cpp` | 1,284 | `CreateDirectoryA`, `GetThreadTimes` for the thread load statistics | `std::filesystem`; `pthread_getcpuclockid` with `clock_gettime` |
| `audio/dsp.cpp` | 916 | `CreateThread` with a 64 MB stack for the DSP self-test | `pthread_attr_setstacksize` |
| `ios/nand.cpp` | 525 | `GetUserDefaultUILanguage` for the console language | `SDL_GetPreferredLocales` |
| `app/main.cpp` | 96 | `SetUnhandledExceptionFilter` crash report | `sigaction` for `SIGSEGV`/`SIGBUS` |

## Order of work

1. **Build system (done 2026-09-29):** split `engine/CMakeLists.txt` into common and per-platform source lists; build the game on Linux with no-op video, audio and input (headless) to boot the game to the main menu, checking `WP_SAVE_FRAME` output through a software copy path. This proves the translated code, fibers, IOS, disc and NAND on Linux.
2. **Fibers (done 2026-09-29):** introduce `wp::fiber` and move `threads.cpp` onto it; run the Windows build on it first to check nothing changes.
3. **SDL3 platform layer:** window, input, audio and locale through SDL3 on every platform, so Windows and Linux share one path (Windows keeps WASAPI quality through SDL's WASAPI backend).
4. **Renderer on SDL_GPU:** port `gx_render.cpp` behind the same interface (`copy_to_texture`, `texture_for`, the TEV pixel shader, EFB copies with write-back, custom textures). Compare frames with the Direct3D 11 renderer using `WP_SAVE_FRAME` and the scripted input paths before switching Windows over.
5. **Launcher on Linux:** use the distribution's GCC, CMake, Ninja and Python instead of downloading the Windows tool chain; `nodtool` from its Linux release; package as an AppImage. Self-update stays Windows-only until a Linux asset exists.
6. **Testers:** the README asks for Linux and macOS testers; macOS follows once SDL_GPU works (Metal) and needs an arm64 check of the fiber layer.

## Checks for each step

- `ctest` passes on both systems (the translator tests are platform independent).
- Scripted runs reach the same screens with identical saved frames at 1x on both systems.
- The Windows build keeps 60 fps and the same guest idle share.
