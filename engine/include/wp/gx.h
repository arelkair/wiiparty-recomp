#pragma once

#include <cstdint>
#include <functional>

namespace wp::gx {

void process();
bool take_finish_interrupt();
void request_capture();
uint64_t frames_drawn();
void run_frame_task(std::function<void()> task);

}
