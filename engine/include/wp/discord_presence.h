#pragma once

#include <cstdint>
#include <string>

namespace wp::discord {

struct Activity {
    std::string details;
    std::string state;
    std::string large_image;
    std::string large_text;
    int64_t start = 0;
    std::string first_label;
    std::string first_url;
    std::string second_label;
    std::string second_url;
};

void start(const char* application_id);
void update(const Activity& activity);
void stop();
bool running();

}
