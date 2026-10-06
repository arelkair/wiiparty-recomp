#pragma once

#include <csetjmp>

#include "wp/cpu.h"

namespace wp {

void save_context(Cpu& c, std::jmp_buf* point);
void resume_context(Cpu& c);
void save_jump(Cpu& c, std::jmp_buf* point);
void resume_jump(Cpu& c);
[[noreturn]] void bad_resume(Cpu& c);

extern uint32_t g_resume_address;
extern uint32_t g_taken_resume_address;

inline uint32_t take_resume_address() {
    g_taken_resume_address = g_resume_address;
    g_resume_address = 0;
    return g_taken_resume_address;
}

}
