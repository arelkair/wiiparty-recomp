#pragma once

#include <cstdint>
#include <functional>

namespace wp::gx {

void process();
bool take_finish_interrupt();
void run_frame_task(std::function<void()> task);

}
