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
    {"input.controls_hint", "Controls window on instruction screens"},
    {"system.interface_language", "Interface language"},
    {"system.language", "Console language"},
    {"system.pal60", "60 Hz (PAL60)"},
    {"system.skip_notices", "Skip the strap notice"},
    {"system.options_menu", "Options menu (F10)"},
    {"audio.mute", "Mute"},
    {"audio.wiimote_speaker", "Wii Remote speaker"},
    {"saves.backups", "Save backups kept"},
    {"hint.title", "Controls on PC"},
    {"hint.pointer", "Pointer"},
    {"hint.mouse", "Mouse"},
    {"hint.dpad", "D-pad"},
    {"hint.shake", "Shake"},
    {"hint.swing", "Swing"},
    {"hint.tilt", "Tilt"},
    {"hint.mouse_motion", "Mouse motion"},
    {"mii.title", "Miis"},
    {"mii.delete", "Delete"},
    {"mii.count", "%1 of 100 Miis"},
    {"mii.damaged", "The Mii database is damaged or is not a Wii database, so it is not edited. Creating a new one keeps a copy of the old file."},
    {"mii.create_database", "Create a new database"},
    {"mii.save_failed", "The Mii database could not be saved."},
    {"mii.import_done", "%1 Mii(s) imported."},
    {"mii.import_filter", "Wii Mii files"},
    {"mii.intro", "Make Miis in the online editor, download them in the Wii format and load them here. The game lists them as Wii console Miis the next time it starts, so close the game first."},
    {"mii.online", "Online Mii editor"},
    {"mii.online_detail", "Opens mii.nxw.pw in your browser. It needs an account and an internet connection; the launcher does not use it."},
    {"mii.open_online", "Open the editor"},
    {"mii.import", "Load Miis"},
    {"mii.choose_file", "Choose a file"},
    {"mii.none", "There are no Miis in the console database yet. Load a Mii file to see it in the game's Wii console list."},
    {"input.player1_keyboard_only", "Player 1: keyboard and mouse only"},
    {"pad.use", "Use this controller"},
    {"pad.player", "Player"},
    {"pad.player_auto", "Automatic"},
    {"pad.view_title", "Controller view"},
    {"pad.view_none", "Connect a controller to see its buttons and sticks here."},
    {"pad.cross", "Cross"},
    {"pad.circle", "Circle"},
    {"pad.square", "Square"},
    {"pad.triangle", "Triangle"},
    {"pad.right_stick_up", "Right stick up"},
    {"pad.right_stick_down", "Right stick down"},
    {"pad.right_stick_left", "Right stick left"},
    {"pad.right_stick_right", "Right stick right"},
    {"pad.motion_help", "Motion without a gyroscope: the left trigger shakes. Hold the left shoulder button and push the right stick to tilt, or press up or down on the D-pad to swing. Each motion can also be given its own button below."},
    {"mii.import_all", "All files"},
    {"mii.import_detail", "Pick or drop a file here: a Wii Mii file (.mii, .miigx, .mae, .rcd, .rsd, any extension, 74 or 76 bytes), several Miis in one file, or a Wii Mii database (RFL_DB.dat). The format is recognised from the contents, not the name. Up to 100 Miis in total."},
    {"mii.import_failed", "That file has no Wii Mii data. Miis from the 3DS, Wii U, Switch or Mii Studio are a different format that this launcher cannot convert yet; use a Wii Mii file or a Wii Mii database (RFL_DB.dat)."},
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
    {"input.controls_hint", "Ventana de controles en las explicaciones"},
    {"system.interface_language", "Idioma de la interfaz"},
    {"system.language", "Idioma de la consola"},
    {"system.pal60", "60 Hz (PAL60)"},
    {"system.skip_notices", "Saltar el aviso de la correa"},
    {"system.options_menu", "Menú de opciones (F10)"},
    {"audio.mute", "Silenciar"},
    {"audio.wiimote_speaker", "Altavoz del Wii Remote"},
    {"saves.backups", "Copias de la partida"},
    {"hint.title", "Controles en PC"},
    {"hint.pointer", "Puntero"},
    {"hint.mouse", "Ratón"},
    {"hint.dpad", "Cruceta"},
    {"hint.shake", "Agitar"},
    {"hint.swing", "Golpe"},
    {"hint.tilt", "Inclinar"},
    {"hint.mouse_motion", "Movimiento con ratón"},
    {"mii.title", "Miis"},
    {"mii.delete", "Eliminar"},
    {"mii.count", "%1 de 100 Miis"},
    {"mii.damaged", "La base de datos de Miis está dañada o no es de Wii, así que no se edita. Crear una nueva conserva una copia del archivo anterior."},
    {"mii.create_database", "Crear una base de datos nueva"},
    {"mii.save_failed", "No se pudo guardar la base de datos de Miis."},
    {"mii.import_done", "%1 Mii(s) importado(s)."},
    {"mii.import_filter", "Archivos de Mii de Wii"},
    {"mii.intro", "Crea Miis en el editor online, descárgalos en el formato de Wii y cárgalos aquí. El juego los muestra como Miis de la consola Wii la próxima vez que arranque, así que cierra antes el juego."},
    {"mii.online", "Editor de Miis online"},
    {"mii.online_detail", "Abre mii.nxw.pw en tu navegador. Requiere cuenta y conexión a internet; el launcher no lo usa."},
    {"mii.open_online", "Abrir el editor"},
    {"mii.import", "Cargar Miis"},
    {"mii.choose_file", "Elegir un archivo"},
    {"mii.none", "Aún no hay Miis en la base de datos de la consola. Carga un archivo de Mii para verlo en la lista de la consola Wii del juego."},
    {"input.player1_keyboard_only", "Jugador 1: solo teclado y ratón"},
    {"pad.use", "Usar este mando"},
    {"pad.player", "Jugador"},
    {"pad.player_auto", "Automático"},
    {"pad.view_title", "Vista del mando"},
    {"pad.view_none", "Conecta un mando para ver aquí sus botones y sticks."},
    {"pad.cross", "Cruz"},
    {"pad.circle", "Círculo"},
    {"pad.square", "Cuadrado"},
    {"pad.triangle", "Triángulo"},
    {"pad.right_stick_up", "Stick derecho arriba"},
    {"pad.right_stick_down", "Stick derecho abajo"},
    {"pad.right_stick_left", "Stick derecho izquierda"},
    {"pad.right_stick_right", "Stick derecho derecha"},
    {"pad.motion_help", "Movimiento sin giroscopio: el gatillo izquierdo sacude. Mantén el botón superior izquierdo y mueve el stick derecho para inclinar, o pulsa arriba o abajo en la cruceta para golpear. Cada movimiento también puede tener su propio botón más abajo."},
    {"mii.import_all", "Todos los archivos"},
    {"mii.import_detail", "Elige o suelta aquí un archivo: un Mii de Wii (.mii, .miigx, .mae, .rcd, .rsd o cualquier extensión, de 74 o 76 bytes), varios Miis en un solo archivo, o una base de datos de Miis de Wii (RFL_DB.dat). El formato se reconoce por el contenido, no por el nombre. Hasta 100 Miis en total."},
    {"mii.import_failed", "Ese archivo no tiene datos de un Mii de Wii. Los Miis de 3DS, Wii U, Switch o Mii Studio son de otro formato que el launcher aún no sabe convertir; usa un archivo de Mii de Wii o una base de datos de Miis de Wii (RFL_DB.dat)."},
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
    {"input.controls_hint", "Xanela de controis nas explicacións"},
    {"system.interface_language", "Idioma da interface"},
    {"system.language", "Idioma da consola"},
    {"system.pal60", "60 Hz (PAL60)"},
    {"system.skip_notices", "Saltar o aviso da correa"},
    {"system.options_menu", "Menú de opcións (F10)"},
    {"audio.mute", "Silenciar"},
    {"audio.wiimote_speaker", "Altofalante do Wii Remote"},
    {"saves.backups", "Copias da partida"},
    {"hint.title", "Controis no PC"},
    {"hint.pointer", "Punteiro"},
    {"hint.mouse", "Rato"},
    {"hint.dpad", "Cruceta"},
    {"hint.shake", "Axitar"},
    {"hint.swing", "Golpe"},
    {"hint.tilt", "Inclinar"},
    {"hint.mouse_motion", "Movemento co rato"},
    {"mii.title", "Miis"},
    {"mii.delete", "Eliminar"},
    {"mii.count", "%1 de 100 Miis"},
    {"mii.damaged", "A base de datos de Miis está danada ou non é de Wii, polo que non se edita. Crear unha nova garda unha copia do ficheiro anterior."},
    {"mii.create_database", "Crear unha base de datos nova"},
    {"mii.save_failed", "Non se puido gardar a base de datos de Miis."},
    {"mii.import_done", "%1 Mii(s) importado(s)."},
    {"mii.import_filter", "Ficheiros de Mii de Wii"},
    {"mii.intro", "Crea Miis no editor online, descárgaos no formato de Wii e cárgaos aquí. O xogo móstraos como Miis da consola Wii a próxima vez que arranque, así que pecha antes o xogo."},
    {"mii.online", "Editor de Miis online"},
    {"mii.online_detail", "Abre mii.nxw.pw no teu navegador. Require conta e conexión a internet; o launcher non o usa."},
    {"mii.open_online", "Abrir o editor"},
    {"mii.import", "Cargar Miis"},
    {"mii.choose_file", "Elixir un ficheiro"},
    {"mii.none", "Aínda non hai Miis na base de datos da consola. Carga un ficheiro de Mii para velo na lista da consola Wii do xogo."},
    {"input.player1_keyboard_only", "Xogador 1: só teclado e rato"},
    {"pad.use", "Usar este mando"},
    {"pad.player", "Xogador"},
    {"pad.player_auto", "Automático"},
    {"pad.view_title", "Vista do mando"},
    {"pad.view_none", "Conecta un mando para ver aquí os seus botóns e sticks."},
    {"pad.cross", "Cruz"},
    {"pad.circle", "Círculo"},
    {"pad.square", "Cadrado"},
    {"pad.triangle", "Triángulo"},
    {"pad.right_stick_up", "Stick dereito arriba"},
    {"pad.right_stick_down", "Stick dereito abaixo"},
    {"pad.right_stick_left", "Stick dereito esquerda"},
    {"pad.right_stick_right", "Stick dereito dereita"},
    {"pad.motion_help", "Movemento sen xiroscopio: o gatillo esquerdo sacode. Mantén o botón superior esquerdo e move o stick dereito para inclinar, ou preme arriba ou abaixo na cruceta para golpear. Cada movemento tamén pode ter o seu propio botón máis abaixo."},
    {"mii.import_all", "Todos os ficheiros"},
    {"mii.import_detail", "Elixe ou solta aquí un ficheiro: un Mii de Wii (.mii, .miigx, .mae, .rcd, .rsd ou calquera extensión, de 74 ou 76 bytes), varios Miis nun só ficheiro, ou unha base de datos de Miis de Wii (RFL_DB.dat). O formato recoñécese polo contido, non polo nome. Ata 100 Miis en total."},
    {"mii.import_failed", "Ese ficheiro non ten datos dun Mii de Wii. Os Miis de 3DS, Wii U, Switch ou Mii Studio son doutro formato que o launcher aínda non sabe converter; usa un ficheiro de Mii de Wii ou unha base de datos de Miis de Wii (RFL_DB.dat)."},
};

constexpr Text kEnglish{"FPS",
                        "DSP",
                        "NATIVE",
                        "INTERP",
                        "OFF",
                        "Options",
                        "Up/Down: choose     Left/Right/Enter or click: change     Esc or right click: close",
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
                        "Arriba/Abajo: elegir     Izquierda/Derecha/Intro o clic: cambiar     Esc o clic derecho: cerrar",
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
                         "Arriba/Abaixo: escoller     Esquerda/Dereita/Intro ou clic: cambiar     Esc ou clic dereito: pechar",
                         "aplícase ao reiniciar",
                         "Si",
                         "Non",
                         "Nativa",
                         "Sistema",
                         kGalicianLabels,
                         sizeof(kGalicianLabels) / sizeof(kGalicianLabels[0])};

std::atomic<const Text*> g_text{&kEnglish};

const char* label_in(const Text& current, const char* key) {
    for (size_t i = 0; i < current.label_count; i++) {
        if (std::strcmp(current.labels[i].key, key) == 0) {
            return current.labels[i].text;
        }
    }
    return key;
}

const Text& text_for(Language language) {
    switch (language) {
    case Language::Spanish:
        return kSpanish;
    case Language::Galician:
        return kGalician;
    default:
        return kEnglish;
    }
}

}

void set_language(Language language) {
    g_text = &text_for(language);
}

Language language_of(const std::string& code) {
    return code == "es" ? Language::Spanish : code == "gl" ? Language::Galician : Language::English;
}

void set_language(const std::string& code) {
    set_language(language_of(code));
}

const Text& text() {
    return *g_text.load();
}

const char* label(const char* key) {
    return label_in(text(), key);
}

const char* label(const char* key, Language language) {
    return label_in(text_for(language), key);
}

}
