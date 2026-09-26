#pragma once

#include <cstddef>

namespace wp::game {

struct Description {
    const char* title;
    const char* short_name;
    const char* data_directory;
    const char* nand_directory;
    const char* settings_file;
    const char* notice_module;
    const char* const* sideways_modules;
    size_t sideways_module_count;
};

const Description& description();

}
