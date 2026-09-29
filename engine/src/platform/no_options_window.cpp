#include "wp/options_window.h"

namespace wp::options {

bool menu_open() {
    return false;
}

void toggle_menu(void*) {}

std::string menu_key(void*, unsigned, bool) {
    return "";
}

std::string menu_wheel(void*, int) {
    return "";
}

std::string menu_click(void*, int, int, bool) {
    return "";
}

std::string poll_gamepads(void*) {
    return "";
}

void refresh_menu(void*) {}

Image render(const Menu&, int) {
    return {};
}

}
