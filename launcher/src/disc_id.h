#pragma once

#include <filesystem>
#include <string>

inline bool valid_disc_id(const std::string& id) {
    if (id.size() != 6) {
        return false;
    }
    for (char letter : id) {
        bool upper = letter >= 'A' && letter <= 'Z';
        bool digit = letter >= '0' && letter <= '9';
        if (!upper && !digit) {
            return false;
        }
    }
    return true;
}

inline bool is_direct_child(const std::filesystem::path& parent, const std::filesystem::path& child) {
    std::filesystem::path normal_parent = parent.lexically_normal();
    std::filesystem::path normal_child = child.lexically_normal();
    if (normal_parent.filename().empty()) {
        normal_parent = normal_parent.parent_path();
    }
    if (normal_child.filename().empty()) {
        normal_child = normal_child.parent_path();
    }
    std::filesystem::path name = normal_child.filename();
    return !name.empty() && name != "." && name != ".." && normal_child.parent_path() == normal_parent;
}
