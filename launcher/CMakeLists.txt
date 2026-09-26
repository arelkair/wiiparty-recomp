cmake_minimum_required(VERSION 3.21)
project(wii_recomp_launcher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
option(WP_STATIC_QT "Link a static Qt into a single executable" OFF)

if(WP_STATIC_QT)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()
find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(LAUNCHER_SOURCES
    src/main.cpp
    src/main_window.cpp
    src/build_runner.cpp
    src/key_capture.cpp
    src/options.cpp
    src/project.cpp
    src/settings_file.cpp
    src/texts.cpp
    src/toolchain.cpp
    ../engine/src/platform/save_backup.cpp
    ../engine/src/input/keymap.cpp
    res/launcher.qrc
)

if(WIN32)
    list(APPEND LAUNCHER_SOURCES res/launcher.rc)
endif()

include(cmake/payload.cmake)
list(APPEND LAUNCHER_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/payload.qrc" src/payload.cpp src/licenses.cpp)

qt_add_executable(launcher WIN32 ${LAUNCHER_SOURCES})
set_target_properties(launcher PROPERTIES OUTPUT_NAME wiipartyrecomp-launcher)
target_include_directories(launcher PRIVATE ../engine/include)
target_compile_definitions(launcher PRIVATE WP_SOURCE_REVISION="${WP_SOURCE_REVISION}")
target_compile_options(launcher PRIVATE -Wall -Wextra)
target_link_libraries(launcher PRIVATE Qt6::Widgets)

get_target_property(WP_QT_TYPE Qt6::Core TYPE)
if(WP_QT_TYPE STREQUAL "STATIC_LIBRARY")
    target_link_options(launcher PRIVATE -static)
    if(TARGET harfbuzz::harfbuzz)
        get_target_property(WP_HARFBUZZ_IMPLIB harfbuzz::harfbuzz IMPORTED_IMPLIB)
        string(REPLACE ".dll.a" ".a" WP_HARFBUZZ_STATIC "${WP_HARFBUZZ_IMPLIB}")
        set_target_properties(harfbuzz::harfbuzz PROPERTIES IMPORTED_LOCATION "${WP_HARFBUZZ_STATIC}" IMPORTED_IMPLIB "${WP_HARFBUZZ_STATIC}")
    endif()
    string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -lbrotlidec -lbrotlicommon -lbz2 -lgraphite2 -lusp10 -lrpcrt4")
    qt_import_plugins(launcher INCLUDE_BY_TYPE imageformats Qt6::QICOPlugin EXCLUDE_BY_TYPE networkinformation tls iconengines)
elseif(WIN32)
    get_target_property(WP_QMAKE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(WP_QT_BIN "${WP_QMAKE}" DIRECTORY)
    add_custom_command(TARGET launcher POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -DEXECUTABLE=$<TARGET_FILE:launcher> -DQT_BIN=${WP_QT_BIN} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_windows.cmake"
        VERBATIM)
endif()
