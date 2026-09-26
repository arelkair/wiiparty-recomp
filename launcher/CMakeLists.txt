cmake_minimum_required(VERSION 3.21)
project(wii_recomp_launcher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(LAUNCHER_SOURCES
    src/main.cpp
    src/main_window.cpp
    src/build_runner.cpp
    src/options.cpp
    src/project.cpp
    src/settings_file.cpp
    src/texts.cpp
    src/toolchain.cpp
    ../engine/src/platform/save_backup.cpp
    res/launcher.qrc
)

if(WIN32)
    list(APPEND LAUNCHER_SOURCES res/launcher.rc)
endif()

add_executable(launcher WIN32 ${LAUNCHER_SOURCES})
set_target_properties(launcher PROPERTIES OUTPUT_NAME wiiparty-launcher)
target_include_directories(launcher PRIVATE ../engine/include)
target_compile_options(launcher PRIVATE -Wall -Wextra)
target_link_libraries(launcher PRIVATE Qt6::Widgets)

if(WIN32)
    get_target_property(WP_QMAKE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(WP_QT_BIN "${WP_QMAKE}" DIRECTORY)
    add_custom_command(TARGET launcher POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -DEXECUTABLE=$<TARGET_FILE:launcher> -DQT_BIN=${WP_QT_BIN} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_windows.cmake"
        VERBATIM)
endif()
