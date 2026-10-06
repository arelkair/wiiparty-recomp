#include "wp/ui_text.h"

#include <atomic>
#include <cstring>

namespace wp::ui {

namespace {

constexpr Label kEnglishLabels[] = {
    {"video.scale", "Internal resolution"},
    {"video.fullscreen", "Full screen"},
    {"video.copy_filter", "Anti-flicker filter"},
    {"video.custom_textures", "Custom textures"},
    {"video.dump_textures", "Dump textures"},
    {"input.gamepads", "Gamepads"},
    {"input.auto_grip", "Automatic grip"},
    {"input.joycon_pairs", "Join Joy-Con pairs"},
    {"input.real_wiimotes", "Real Wii Remotes"},
    {"input.real_wiimote_mouse", "Aim Wii Remote 1 with the mouse"},
    {"input.wake_on_mouse", "Reconnect with the mouse"},
    {"input.hide_cursor", "Hide the mouse cursor"},
    {"system.interface_language", "Interface language"},
    {"system.language", "Console language"},
    {"system.pal60", "60 Hz (PAL60)"},
    {"system.skip_notices", "Skip the strap notice"},
    {"system.options_menu", "Options menu (F10)"},
    {"audio.mute", "Mute"},
    {"saves.backups", "Save backups kept"},
};

constexpr Label kSpanishLabels[] = {
    {"video.scale", "Resolución interna"},
    {"video.fullscreen", "Pantalla completa"},
    {"video.copy_filter", "Filtro anti-parpadeo"},
    {"video.custom_textures", "Texturas personalizadas"},
    {"video.dump_textures", "Volcar texturas"},
    {"input.gamepads", "Mandos"},
    {"input.auto_grip", "Agarre automático"},
    {"input.joycon_pairs", "Unir parejas de Joy-Con"},
    {"input.real_wiimotes", "Wii Remotes reales"},
    {"input.real_wiimote_mouse", "Apuntar el Wii Remote 1 con el ratón"},
    {"input.wake_on_mouse", "Reconectar con el ratón"},
    {"input.hide_cursor", "Ocultar el cursor del ratón"},
    {"system.interface_language", "Idioma de la interfaz"},
    {"system.language", "Idioma de la consola"},
    {"system.pal60", "60 Hz (PAL60)"},
    {"system.skip_notices", "Saltar el aviso de la correa"},
    {"system.options_menu", "Menú de opciones (F10)"},
    {"audio.mute", "Silenciar"},
    {"saves.backups", "Copias de la partida"},
};

constexpr Label kGalicianLabels[] = {
    {"video.scale", "Resolución interna"},
    {"video.fullscreen", "Pantalla completa"},
    {"video.copy_filter", "Filtro antiparpadeo"},
    {"video.custom_textures", "Texturas personalizadas"},
    {"video.dump_textures", "Envorcar texturas"},
    {"input.gamepads", "Mandos"},
    {"input.auto_grip", "Agarre automático"},
    {"input.joycon_pairs", "Unir parellas de Joy-Con"},
    {"input.real_wiimotes", "Wii Remotes reais"},
    {"input.real_wiimote_mouse", "Apuntar o Wii Remote 1 co rato"},
    {"input.wake_on_mouse", "Reconectar co rato"},
    {"input.hide_cursor", "Agochar o cursor do rato"},
    {"system.interface_language", "Idioma da interface"},
    {"system.language", "Idioma da consola"},
    {"system.pal60", "60 Hz (PAL60)"},
    {"system.skip_notices", "Saltar o aviso da correa"},
    {"system.options_menu", "Menú de opcións (F10)"},
    {"audio.mute", "Silenciar"},
    {"saves.backups", "Copias da partida"},
};

constexpr Text kEnglish{"FPS",
                        "DSP",
                        "NATIVE",
                        "INTERP",
                        "OFF",
                        "Options",
                        "Up/Down: choose     Left/Right/Enter: change     Esc: close",
                        "applies on restart",
                        "On",
                        "Off",
                        "Native",
                        "System",
                        kEnglishLabels,
                        sizeof(kEnglishLabels) / sizeof(kEnglishLabels[0])};

constexpr Text kSpanish{"FPS",
                        "DSP",
                        "NATIVO",
                        "INTERP",
                        "APAGADO",
                        "Opciones",
                        "Arriba/Abajo: elegir     Izquierda/Derecha/Intro: cambiar     Esc: cerrar",
                        "se aplica al reiniciar",
                        "Sí",
                        "No",
                        "Nativa",
                        "Sistema",
                        kSpanishLabels,
                        sizeof(kSpanishLabels) / sizeof(kSpanishLabels[0])};

constexpr Text kGalician{"FPS",
                         "DSP",
                         "NATIVO",
                         "INTERP",
                         "APAGADO",
                         "Opcións",
                         "Arriba/Abaixo: escoller     Esquerda/Dereita/Intro: cambiar     Esc: pechar",
                         "aplícase ao reiniciar",
                         "Si",
                         "Non",
                         "Nativa",
                         "Sistema",
                         kGalicianLabels,
                         sizeof(kGalicianLabels) / sizeof(kGalicianLabels[0])};

std::atomic<const Text*> g_text{&kEnglish};

}

void set_language(Language language) {
    switch (language) {
    case Language::English:
        g_text = &kEnglish;
        break;
    case Language::Spanish:
        g_text = &kSpanish;
        break;
    case Language::Galician:
        g_text = &kGalician;
        break;
    }
}

void set_language(const std::string& code) {
    set_language(code == "es" ? Language::Spanish : code == "gl" ? Language::Galician : Language::English);
}

const Text& text() {
    return *g_text.load();
}

const char* label(const char* key) {
    const Text& current = text();
    for (size_t i = 0; i < current.label_count; i++) {
        if (std::strcmp(current.labels[i].key, key) == 0) {
            return current.labels[i].text;
        }
    }
    return key;
}

}
