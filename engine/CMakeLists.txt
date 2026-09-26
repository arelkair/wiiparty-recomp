add_library(wp_engine STATIC
    src/core/runtime.cpp
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
    src/gpu/gx_render.cpp
    src/gpu/custom_textures.cpp
    src/gpu/gx_state.cpp
    src/audio/dsp.cpp
    src/audio/audio.cpp
    src/ios/ios.cpp
    src/ios/ipc.cpp
    src/ios/nand.cpp
    src/ios/disc.cpp
    src/input/bluetooth.cpp
    src/input/wiimote.cpp
    src/input/input.cpp
    src/input/keymap.cpp
    src/platform/video.cpp
    src/platform/save_backup.cpp
    src/platform/screenshot.cpp
    src/platform/settings.cpp
    src/platform/ui_text.cpp
)
target_include_directories(wp_engine PUBLIC include)
target_compile_definitions(wp_engine PRIVATE WP_BUILD="${WP_BUILD}")
target_link_libraries(wp_engine PUBLIC wp_dolphin_dsp gdi32 user32 d3d11 d3dcompiler dxgi ole32 windowscodecs)
if(WP_TRACE)
    target_compile_definitions(wp_engine PUBLIC WP_TRACE)
endif()
target_compile_options(wp_engine PRIVATE -Wall -Wextra)

set(WP_SDL3_DIR "${CMAKE_SOURCE_DIR}/build/deps/SDL3-3.4.16/x86_64-w64-mingw32" CACHE PATH "SDL3 development files for gamepad support")
find_package(SDL3 CONFIG QUIET GLOBAL PATHS "${WP_SDL3_DIR}/lib/cmake/SDL3" NO_DEFAULT_PATH)
if(SDL3_FOUND)
    target_sources(wp_engine PRIVATE src/input/gamepad.cpp)
    target_link_libraries(wp_engine PUBLIC SDL3::SDL3)
else()
    target_sources(wp_engine PRIVATE src/input/no_gamepad.cpp)
endif()

add_library(wp_app INTERFACE)
target_sources(wp_app INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/res/dsp_roms.rc")
target_link_options(wp_app INTERFACE -Wl,--stack,536870912)
