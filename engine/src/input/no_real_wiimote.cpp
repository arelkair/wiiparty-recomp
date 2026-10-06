#include "wp/real_wiimote.h"

namespace wp::real_wiimote {

void start() {}

bool present(uint32_t) {
    return false;
}

bool take_wake(uint32_t) {
    return false;
}

void reset(uint32_t) {}

void output_report(uint32_t, const uint8_t*, uint32_t) {}

void update(uint32_t, const Sender&) {}

}
