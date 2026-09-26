#pragma once

#include <string>

namespace wp::settings {

void load(const std::string& path);
bool flag(const char* key, const char* variable);
int number(const char* key, const char* variable);
std::string text(const char* key, const char* variable);
void store(const char* key, const std::string& value);

}
