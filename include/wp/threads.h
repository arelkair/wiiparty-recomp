#pragma once

#include <csetjmp>

#include "wp/cpu.h"

namespace wp {

void init_threads(Cpu& c);
void save_context(Cpu& c, std::jmp_buf* point);
void resume_context(Cpu& c);
void load_context(Cpu& c);
void save_jump(Cpu& c, std::jmp_buf* point);
void resume_jump(Cpu& c);
void long_jump(Cpu& c);
void print_thread_stacks();
uint32_t symbol_address(const char* name);

}
