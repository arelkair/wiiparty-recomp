#pragma once

#include <cstddef>
#include <cstdint>

#include "wp/cpu.h"

namespace wp {

struct FunctionEntry {
    uint32_t address;
    void (*function)(Cpu&);
};

struct NameEntry {
    uint32_t address;
    const char* name;
};

extern const FunctionEntry g_function_table[];
extern const size_t g_function_count;
extern const FunctionEntry g_resume_table[];
extern const size_t g_resume_count;
extern const NameEntry g_name_table[];
extern const size_t g_name_count;

const char* find_name(uint32_t address);

}
