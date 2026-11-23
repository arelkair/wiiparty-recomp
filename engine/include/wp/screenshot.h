#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace wp::screenshot {

constexpr const char* kFolder = "screenshots";

std::string file_name(const char* prefix, const std::tm& time, int attempt);
void fit_aspect(std::vector<uint32_t>& pixels, uint32_t& width, uint32_t& height, double aspect);

}
