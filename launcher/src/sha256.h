#pragma once

#include <filesystem>
#include <string>

std::string sha256_of(const std::filesystem::path& path);
