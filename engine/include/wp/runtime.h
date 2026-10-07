#pragma once

#include <csetjmp>
#include <cstdint>

#include "wp/cpu.h"

namespace wp {

[[noreturn]] void missing_function(Cpu& c, uint32_t address);
[[noreturn]] void fatal_error(const char* message);
bool decrementer_due();
void decrementer_fired();
void print_call_stack();
void start_profiler();
void print_profile();
bool call_module_function(Cpu& c, uint32_t address);
void (*find_module_resume(uint32_t address))(Cpu&);
void describe_loaded_modules(uint32_t address);
uint32_t external_address(uint32_t module_identifier, uint32_t section, uint32_t offset);
const char* module_name_at(uint32_t address);
bool module_loaded(const char* name);
void init_threads(Cpu& c);
void load_context(Cpu& c);
void enter_exception_context(uint32_t context);
void forget_saved_context(uint32_t context, std::jmp_buf* point);
void long_jump(Cpu& c);
void print_thread_stacks();
void print_guest_registers();
uint32_t symbol_address(const char* name);
void interrupt_left();

}
