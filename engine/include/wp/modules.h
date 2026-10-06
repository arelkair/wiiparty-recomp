#pragma once

#include <cstdint>

#include "wp/cpu.h"

namespace wp {

struct ModuleFunction {
    uint32_t section;
    uint32_t offset;
    void (*function)(Cpu&);
};

struct ModuleDescriptor {
    const char* name;
    uint32_t identifier;
    uint32_t signature;
    uint32_t section_count;
    uint32_t* bases;
    const ModuleFunction* functions;
    size_t function_count;
    const ModuleFunction* resumes;
    size_t resume_count;
};

extern const ModuleDescriptor* const g_module_table[];
extern const size_t g_module_count;

}
