#pragma once

#include <filesystem>
#include <string>

std::string pref(const char* key);
void set_pref(const char* key, const std::string& value);
std::filesystem::path portable_folder();
std::filesystem::path installed_prefs_file();
void export_prefs(const std::filesystem::path& file);
