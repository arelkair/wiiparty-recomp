#pragma once

namespace wp::ui {

enum class Language { English };

struct Text {
    const char* fps;
    const char* dsp;
    const char* dsp_native;
    const char* dsp_interpreted;
    const char* dsp_stopped;
};

void set_language(Language language);
const Text& text();

}
