#pragma once

#include "wp/cpu.h"

namespace wp {

using HleFunction = void (*)(Cpu&);

HleFunction find_replacement(const char* name);

}
