#include "wp/ui_text.h"

namespace wp::ui {

namespace {

constexpr Text kEnglish{"FPS", "DSP", "NATIVE", "INTERP", "OFF"};

const Text* g_text = &kEnglish;

}

void set_language(Language language) {
    switch (language) {
    case Language::English:
        g_text = &kEnglish;
        break;
    }
}

const Text& text() {
    return *g_text;
}

}
