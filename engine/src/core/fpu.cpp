#include "wp/cpu.h"

namespace wp {

bool g_hardware_fma = __builtin_cpu_supports("fma");

__attribute__((target("fma"))) double hardware_fma(double a, double b, double c) {
    return __builtin_fma(a, b, c);
}

}
