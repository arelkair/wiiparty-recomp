#pragma once

#include "wp/cpu.h"

namespace wp {

using HleFunction = void (*)(Cpu&);

HleFunction find_replacement(const char* name);
bool ipc_pending();
void ipc_deliver(Cpu& c);

}
