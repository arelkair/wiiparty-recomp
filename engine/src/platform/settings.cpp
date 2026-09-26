#include "wp/settings.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

#include "wp/keymap.h"

namespace wp::settings {

namespace {

struct Option {
    std::string key;
    std::string fallback;
};

const Option kBaseOptions[] = {
    {"video.scale", "1"},
    {"video.fullscreen", "0"},
    {"video.copy_filter", "1"},
    {"input.gamepads", "1"},
    {"input.auto_grip", "1"},
    {"input.wake_on_mouse", "1"},
    {"input.hide_cursor", "0"},
    {"system.language", "auto"},
    {"system.pal60", "1"},
    {"system.skip_notices", "1"},
    {"audio.mute", "0"},
};

const std::vector<Option>& options() {
    static const std::vector<Option> list = [] {
        std::vector<Option> result(std::begin(kBaseOptions), std::end(kBaseOptions));
        for (size_t i = 0; i < keymap::kActionCount; i++) {
            result.push_back({keymap::setting_key(i), keymap::action(i).defaults});
        }
        return result;
    }();
    return list;
}

std::mutex g_mutex;
std::string g_path;
std::map<std::string, std::string> g_values;

std::string fallback_of(const char* key) {
    for (const Option& option : options()) {
        if (option.key == key) {
            return option.fallback;
        }
    }
    return "";
}

std::string trimmed(const std::string& text) {
    size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

void write_file() {
    if (g_path.empty()) {
        return;
    }
    std::ostringstream out;
    std::string section;
    for (const Option& option : options()) {
        const std::string& key = option.key;
        size_t dot = key.find('.');
        std::string group = key.substr(0, dot);
        if (group != section) {
            if (!section.empty()) {
                out << '\n';
            }
            out << '[' << group << "]\n";
            section = group;
        }
        out << key.substr(dot + 1) << '=' << g_values[key] << '\n';
    }
    std::ofstream file(g_path, std::ios::binary | std::ios::trunc);
    file << out.str();
}

std::string value_of(const char* key, const char* variable) {
    if (variable) {
        if (const char* setting = std::getenv(variable)) {
            return setting;
        }
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_values.find(key);
    return it != g_values.end() ? it->second : fallback_of(key);
}

}

void load(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_path = path;
    for (const Option& option : options()) {
        g_values[option.key] = option.fallback;
    }
    std::ifstream file(path, std::ios::binary);
    std::string line;
    std::string section;
    while (std::getline(file, line)) {
        line = trimmed(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trimmed(line.substr(1, line.size() - 2));
            continue;
        }
        size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        std::string key = section + "." + trimmed(line.substr(0, equals));
        if (g_values.count(key)) {
            g_values[key] = trimmed(line.substr(equals + 1));
        }
    }
    write_file();
}

bool flag(const char* key, const char* variable) {
    std::string value = value_of(key, variable);
    return !(value == "0" || value == "false" || value == "off" || value == "no");
}

int number(const char* key, const char* variable) {
    return std::atoi(value_of(key, variable).c_str());
}

std::string text(const char* key, const char* variable) {
    return value_of(key, variable);
}

void store(const char* key, const std::string& value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_values[key] = value;
    write_file();
}

}
