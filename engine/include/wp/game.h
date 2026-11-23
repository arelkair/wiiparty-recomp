#pragma once

#include <cstddef>

namespace wp::game {

constexpr size_t kPresenceLanguages = 7;

struct PresenceLabels {
    const char* text[kPresenceLanguages];
};

struct PresenceRule {
    const char* module_prefix;
    PresenceLabels labels;
};

struct Description {
    const char* title;
    const char* short_name;
    const char* data_directory;
    const char* nand_directory;
    const char* settings_file;
    const char* textures_directory;
    const char* notice_module;
    const char* const* sideways_modules;
    size_t sideways_module_count;
    const char* instruction_module;
    const char* minigame_prefix;
    const char* discord_application_id;
    PresenceLabels presence_default;
    const PresenceRule* presence_rules;
    size_t presence_rule_count;
};

const Description& description();

}
