add_library(wp_engine STATIC
    src/core/runtime.cpp
    src/core/fpu.cpp
    src/core/fiber.cpp
    src/core/big_stack.cpp
    src/core/dol.cpp
    src/core/boot.cpp
    src/core/hle.cpp
    src/core/threads.cpp
    src/core/interrupts.cpp
    src/core/modules.cpp
    src/core/format.cpp
    src/core/log.cpp
    src/gpu/gx.cpp
    src/gpu/gx_lighting.cpp
    src/gpu/custom_textures.cpp
    src/gpu/gx_state.cpp
    src/gpu/gx_render_common.cpp
    src/audio/dsp.cpp
    src/audio/audio.cpp
    src/audio/dsp_roms.cpp
    src/ios/ios.cpp
    src/ios/ipc.cpp
    src/ios/nand.cpp
    src/ios/disc.cpp
    src/input/bluetooth.cpp
    src/input/wiimote.cpp
    src/input/input.cpp
    src/input/keymap.cpp
    src/platform/png.cpp
    src/platform/save_backup.cpp
    src/platform/screenshot.cpp
    src/platform/settings.cpp
    src/platform/ui_text.cpp
    src/platform/options.cpp
    src/platform/options_window.cpp
)
if(WIN32)
    target_sources(wp_engine PRIVATE src/gpu/gx_render.cpp src/platform/video.cpp src/platform/options_render_gdi.cpp)
else()
    target_sources(wp_engine PRIVATE src/platform/options_render_stb.cpp)
    target_include_directories(wp_engine SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/third_party/stb")
endif()
target_include_directories(wp_engine PUBLIC include)
target_compile_definitions(wp_engine PRIVATE WP_BUILD="${WP_BUILD}"
    WP_DSP_IROM_PATH="${CMAKE_SOURCE_DIR}/third_party/dolphin/Data/Sys/GC/dsp_rom.bin"
    WP_DSP_COEF_PATH="${CMAKE_SOURCE_DIR}/third_party/dolphin/Data/Sys/GC/dsp_coef.bin")
set_source_files_properties(src/audio/dsp_roms.cpp PROPERTIES OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/third_party/dolphin/Data/Sys/GC/dsp_rom.bin;${CMAKE_SOURCE_DIR}/third_party/dolphin/Data/Sys/GC/dsp_coef.bin")
target_link_libraries(wp_engine PUBLIC wp_dolphin_dsp)
if(WIN32)
    target_link_libraries(wp_engine PUBLIC gdi32 user32 d3d11 d3dcompiler dxgi ole32 windowscodecs dwmapi)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(wp_engine PUBLIC Threads::Threads)
endif()
if(WP_TRACE)
    target_compile_definitions(wp_engine PUBLIC WP_TRACE)
endif()
target_compile_options(wp_engine PRIVATE -Wall -Wextra)

if(WIN32)
    set(WP_SDL3_DIR "${CMAKE_SOURCE_DIR}/build/deps/SDL3-3.4.16/x86_64-w64-mingw32" CACHE PATH "SDL3 development files for gamepad support")
    find_package(SDL3 CONFIG QUIET GLOBAL PATHS "${WP_SDL3_DIR}/lib/cmake/SDL3" NO_DEFAULT_PATH)
else()
    find_package(SDL3 CONFIG QUIET GLOBAL)
    option(WP_FETCH_SDL3 "Download and build SDL3 when the system has none" ON)
    if(NOT SDL3_FOUND AND WP_FETCH_SDL3)
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(sdl3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
            URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
            DOWNLOAD_EXTRACT_TIMESTAMP ON)
        FetchContent_MakeAvailable(sdl3)
        set(SDL3_FOUND TRUE)
    endif()
endif()
if(SDL3_FOUND)
    target_sources(wp_engine PRIVATE src/input/gamepad.cpp)
    if(TARGET SDL3::SDL3)
        target_link_libraries(wp_engine PUBLIC SDL3::SDL3)
    else()
        target_link_libraries(wp_engine PUBLIC SDL3::SDL3-static)
    endif()
else()
    target_sources(wp_engine PRIVATE src/input/no_gamepad.cpp)
endif()
if(NOT WIN32)
    if(SDL3_FOUND)
        target_sources(wp_engine PRIVATE src/gpu/gx_render_gl.cpp src/platform/video_sdl.cpp)
        target_compile_definitions(wp_engine PRIVATE WP_SDL_PLATFORM)
    else()
        target_sources(wp_engine PRIVATE src/gpu/null_render.cpp src/platform/video_headless.cpp)
    endif()
endif()

add_library(wp_app INTERFACE)
target_sources(wp_app INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")
if(WIN32)
    target_link_options(wp_app INTERFACE -Wl,--stack,536870912)
endif()
