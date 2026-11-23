#pragma once

#include <string>

namespace wp::presence {

void modules_changed(const std::string& modules);
void poll();

}
