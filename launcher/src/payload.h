#pragma once

#include <filesystem>
#include <string>

bool payload_available();
std::string stored_revision(const std::filesystem::path& root);
bool extract_payload(const std::filesystem::path& root, std::string& message);
