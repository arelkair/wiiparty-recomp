#include "texts.h"

#include <cstring>

namespace {

constexpr Texts kEnglish = {
    "Game",
    "Settings",
    "Saves",
    "Controls",
    "Licenses",
    "Ready to play.",
    "Choose your copy of Wii Party to install it.",
    "Play",
    "Options",
    "Full screen",
    "Screenshot",
    "Frame capture",
    "An update is ready",
    "This launcher brings a newer version of the game. Install it to rebuild the game; your saves and settings stay as they are.",
    "Update",
    "Installing translates the game from your own disc into a native program. Missing tools are downloaded from their official sites; nothing from the game is downloaded and nothing leaves your computer. It takes about 10 minutes on a 6-core processor.",
    "Install folder",
    "Change",
    "Disc",
    "No disc chosen",
    "Already extracted",
    "Choose",
    "Wii disc images",
    "Install",
    "Installing",
    "Cancel",
    "Try again",
    "Back",
    "Show details",
    "Hide details",
    "About %1 minutes left",
    "About a minute left",
    "Choose your copy of Wii Party first.",
    "Needs about %1 GB of free space. That drive has %2 GB free.",
    "Not enough free space: the installation needs about %1 GB and that drive has %2 GB free.",
    "The install folder path is too long for the compiler. Choose a folder with a shorter path, for example C:\\Games.",
    "%1: %2, ready",
    "%1: missing or not compatible, it will be downloaded",
    "Compiling with %1 parallel jobs.",
    "Prepare the build files",
    "Find %1",
    "Download %1",
    "Check %1",
    "Unpack %1",
    "Finish %1",
    "Extract the disc",
    "Download SDL3 for gamepads",
    "Unpack the code modules",
    "Translate the main executable",
    "Translate the code modules",
    "Link the modules to the main executable",
    "Translate the audio microcode",
    "Configure the build",
    "Compile the game",
    "The game is installed.",
    "The installation stopped at a failed step. The details show the error.",
    "The installation was cancelled.",
    "Missing tool: %1",
    "%1 is missing. Install it with your system's package manager and try again.",
    "%1 does not match its published checksum and was deleted. Try again.",
    "Could not write the build files to %1.",
    "Build files written to %1.",
    "The game could not be started.",
    "PC improvements, kept apart from the translated game. They apply the next time the game starts; F10 opens them while playing, where most apply at once.",
    "Video",
    "Input",
    "System",
    "Audio",
    "Saves",
    "Copies of your progress, made automatically. Restoring one replaces the current save, which is first kept as a new copy.",
    "No backups yet. The first one is made the next time the game starts with a save.",
    "Automatic backups are off. Turn them on in Settings.",
    "Open folder",
    "Restore",
    "%1 KB",
    "Restore this backup?",
    "The backup from %1 replaces the current save. Close the game first; the current save is kept as a new backup.",
    "The backup from %1 is now the save.",
    "The save already matches the backup from %1.",
    "The backup could not be restored: %1",
    "Keyboard and mouse for Wii Remote 1. Click a binding, then press a key, click or scroll to add it. F10, F11 and F12 are reserved.",
    "Reset to defaults",
    "Clear",
    "Not set",
    "Press a key, click or scroll",
    "F10, F11 and F12 are reserved",
    "Wii Party Recomp is free software under the GPL-3.0-or-later. The launcher includes the libraries below, each under its own license.",
    "Wii Party Recomp",
    "Third-party notices",
    "GCC runtime",
};

constexpr Texts kSpanish = {
    "Juego",
    "Opciones",
    "Partidas",
    "Controles",
    "Licencias",
    "Listo para jugar.",
    "Elige tu copia de Wii Party para instalarlo.",
    "Jugar",
    "Opciones",
    "Pantalla completa",
    "Captura",
    "Captura de fotograma",
    "Hay una actualización",
    "Este lanzador trae una versión más nueva del juego. Instálala para recompilarlo; tus partidas y opciones se conservan.",
    "Actualizar",
    "La instalación traduce el juego desde tu propio disco a un programa nativo. Las herramientas que falten se descargan de sus webs oficiales; no se descarga nada del juego y nada sale de tu ordenador. Tarda unos 10 minutos con un procesador de 6 núcleos.",
    "Carpeta de instalación",
    "Cambiar",
    "Disco",
    "Ningún disco elegido",
    "Ya extraído",
    "Elegir",
    "Imágenes de disco de Wii",
    "Instalar",
    "Instalando",
    "Cancelar",
    "Reintentar",
    "Volver",
    "Mostrar detalles",
    "Ocultar detalles",
    "Quedan unos %1 minutos",
    "Queda un minuto",
    "Elige primero tu copia de Wii Party.",
    "Necesita unos %1 GB libres. Esa unidad tiene %2 GB libres.",
    "No hay espacio suficiente: la instalación necesita unos %1 GB y esa unidad tiene %2 GB libres.",
    "La ruta de la carpeta de instalación es demasiado larga para el compilador. Elige una carpeta con una ruta más corta, por ejemplo C:\\Juegos.",
    "%1: %2, lista",
    "%1: falta o no es compatible, se descargará",
    "Compilando con %1 procesos en paralelo.",
    "Preparar los archivos de compilación",
    "Buscar %1",
    "Descargar %1",
    "Comprobar %1",
    "Descomprimir %1",
    "Terminar %1",
    "Extraer el disco",
    "Descargar SDL3 para los mandos",
    "Descomprimir los módulos de código",
    "Traducir el ejecutable principal",
    "Traducir los módulos de código",
    "Enlazar los módulos con el ejecutable",
    "Traducir el microcódigo de audio",
    "Preparar la compilación",
    "Compilar el juego",
    "El juego está instalado.",
    "La instalación se ha detenido en un paso con error. Los detalles muestran el fallo.",
    "La instalación se ha cancelado.",
    "Falta la herramienta: %1",
    "Falta %1. Instálalo con el gestor de paquetes del sistema y vuelve a intentarlo.",
    "%1 no coincide con su suma de comprobación publicada y se ha borrado. Vuelve a intentarlo.",
    "No se han podido escribir los archivos de compilación en %1.",
    "Archivos de compilación escritos en %1.",
    "No se ha podido abrir el juego.",
    "Mejoras de PC, separadas del juego traducido. Se aplican la próxima vez que se abra el juego; F10 las abre durante la partida, donde casi todas se aplican al momento.",
    "Vídeo",
    "Entrada",
    "Sistema",
    "Sonido",
    "Partidas",
    "Copias de tu progreso, hechas automáticamente. Restaurar una sustituye la partida actual, que antes se guarda como una copia nueva.",
    "Aún no hay copias. La primera se hará la próxima vez que el juego se abra con una partida guardada.",
    "Las copias automáticas están desactivadas. Actívalas en Opciones.",
    "Abrir carpeta",
    "Restaurar",
    "%1 KB",
    "¿Restaurar esta copia?",
    "La copia del %1 sustituye a la partida actual. Cierra antes el juego; la partida actual se guarda como una copia nueva.",
    "La copia del %1 es ahora la partida.",
    "La partida ya coincide con la copia del %1.",
    "No se ha podido restaurar la copia: %1",
    "Teclado y ratón para el Wii Remote 1. Pulsa una asignación y después una tecla, un clic o la rueda para añadirla. F10, F11 y F12 están reservadas.",
    "Restablecer",
    "Quitar",
    "Sin asignar",
    "Pulsa una tecla, haz clic o gira la rueda",
    "F10, F11 y F12 están reservadas",
    "Wii Party Recomp es software libre bajo la GPL-3.0-or-later. El lanzador incluye las bibliotecas de abajo, cada una con su propia licencia.",
    "Wii Party Recomp",
    "Avisos de terceros",
    "Runtime de GCC",
};

struct Detail {
    const char* key;
    const char* english;
    const char* spanish;
};

constexpr Detail kDetails[] = {
    {"video.scale", "Renders the game at a multiple of the console's resolution.", "Dibuja el juego a un múltiplo de la resolución de la consola."},
    {"video.fullscreen", "Starts in borderless full screen. F11 also switches it while playing.", "Empieza en pantalla completa sin bordes. F11 también la cambia durante la partida."},
    {"video.copy_filter", "The soft vertical filter the game uses on the console. Turn it off for a sharper picture.",
     "El suave filtro vertical que usa el juego en la consola. Desactívalo para una imagen más nítida."},
    {"video.custom_textures", "Uses the replacement textures in games/wiiparty/textures/load, with Dolphin's file names.",
     "Usa las texturas de reemplazo de games/wiiparty/textures/load, con los nombres de archivo de Dolphin."},
    {"video.dump_textures", "Saves each texture the game uses to games/wiiparty/textures/dump, for making texture packs.",
     "Guarda cada textura que usa el juego en games/wiiparty/textures/dump, para crear packs de texturas."},
    {"input.gamepads", "Up to four gamepads act as Wii Remotes 1 to 4.", "Hasta cuatro mandos funcionan como los Wii Remote 1 a 4."},
    {"input.auto_grip", "Holds the emulated Wii Remote sideways in the minigames that ask for it.",
     "Sujeta el Wii Remote emulado en horizontal en los minijuegos que lo piden."},
    {"input.wake_on_mouse", "Moving the mouse reconnects a Wii Remote the game disconnected for inactivity.",
     "Mover el ratón reconecta un Wii Remote que el juego desconectó por inactividad."},
    {"input.hide_cursor", "Hides the system cursor over the game; the game draws its own pointer.",
     "Oculta el cursor del sistema sobre el juego; el juego dibuja su propio puntero."},
    {"system.interface_language", "The language of this launcher and of the F10 menu.", "El idioma de este lanzador y del menú F10."},
    {"system.language", "The language of the virtual console, which the game follows.", "El idioma de la consola virtual, que el juego sigue."},
    {"system.pal60", "The console's own PAL60 setting. Off runs the game at 50 Hz.", "El ajuste PAL60 de la propia consola. Desactivado, el juego va a 50 Hz."},
    {"system.skip_notices", "Presses A on the Wii Remote strap notice at start, as a player would.",
     "Pulsa A en el aviso de la correa del Wii Remote al empezar, como haría un jugador."},
    {"system.options_menu", "F10, or Start with Guide or Back on a gamepad, opens these settings over the game.",
     "F10, o Start con Guide o Back en un mando, abre estas opciones sobre el juego."},
    {"audio.mute", "Silences the game's sound.", "Quita el sonido del juego."},
    {"saves.backups", "Copies of the save to keep. One is made at every start when the save changed, and at most once a minute after the game saves.",
     "Copias de la partida que se guardan. Se hace una al abrir el juego si la partida ha cambiado, y como mucho una por minuto después de que el juego guarde."},
};

struct Action {
    const char* key;
    const char* english;
    const char* spanish;
    const char* english_detail;
    const char* spanish_detail;
};

constexpr Action kActions[] = {
    {"a", "A button", "Botón A", nullptr, nullptr},
    {"b", "B button", "Botón B", nullptr, nullptr},
    {"one", "1 button", "Botón 1", nullptr, nullptr},
    {"two", "2 button", "Botón 2", nullptr, nullptr},
    {"plus", "+ button", "Botón +", nullptr, nullptr},
    {"minus", "- button", "Botón -", nullptr, nullptr},
    {"home", "HOME button", "Botón HOME", nullptr, nullptr},
    {"up", "D-pad up", "Cruceta arriba", "Left on screen when the remote is held sideways.", "Izquierda en pantalla con el mando en horizontal."},
    {"down", "D-pad down", "Cruceta abajo", "Right on screen when the remote is held sideways.", "Derecha en pantalla con el mando en horizontal."},
    {"left", "D-pad left", "Cruceta izquierda", "Down on screen when the remote is held sideways.", "Abajo en pantalla con el mando en horizontal."},
    {"right", "D-pad right", "Cruceta derecha", "Up on screen when the remote is held sideways.", "Arriba en pantalla con el mando en horizontal."},
    {"shake", "Shake", "Agitar", nullptr, nullptr},
    {"swing_up", "Swing up", "Mover hacia arriba", "The mouse wheel gives a short swing.", "La rueda del ratón da un movimiento corto."},
    {"swing_down", "Swing down", "Mover hacia abajo", "The mouse wheel gives a short swing.", "La rueda del ratón da un movimiento corto."},
    {"tilt_left", "Tilt left", "Inclinar a la izquierda", nullptr, nullptr},
    {"tilt_right", "Tilt right", "Inclinar a la derecha", nullptr, nullptr},
    {"tilt_up", "Tilt up", "Inclinar hacia arriba", nullptr, nullptr},
    {"tilt_down", "Tilt down", "Inclinar hacia abajo", nullptr, nullptr},
    {"grip", "Swap upright and sideways grip", "Cambiar entre agarre vertical y horizontal", nullptr, nullptr},
    {"screenshot", "Screenshot", "Captura de pantalla", "Saves the picture of the game as PNG in the screenshots folder.",
     "Guarda la imagen del juego como PNG en la carpeta screenshots."},
};

bool g_spanish = false;

const Action* find_action(const char* key) {
    for (const Action& action : kActions) {
        if (std::strcmp(action.key, key) == 0) {
            return &action;
        }
    }
    return nullptr;
}

}

void use_spanish(bool spanish) {
    g_spanish = spanish;
}

const Texts& texts() {
    return g_spanish ? kSpanish : kEnglish;
}

std::string format(const char* pattern, const std::string& first, const std::string& second) {
    std::string result;
    for (const char* c = pattern; *c; c++) {
        if (c[0] == '%' && (c[1] == '1' || c[1] == '2')) {
            result += c[1] == '1' ? first : second;
            c++;
        } else {
            result += *c;
        }
    }
    return result;
}

const char* option_detail(const std::string& key) {
    for (const Detail& detail : kDetails) {
        if (key == detail.key) {
            return g_spanish ? detail.spanish : detail.english;
        }
    }
    return "";
}

const char* group_name(const std::string& key) {
    const Texts& t = texts();
    std::string group = key.substr(0, key.find('.'));
    if (group == "video") {
        return t.group_video;
    }
    if (group == "input") {
        return t.group_input;
    }
    if (group == "audio") {
        return t.group_audio;
    }
    if (group == "saves") {
        return t.group_saves;
    }
    return t.group_system;
}

const char* action_name(const char* key) {
    const Action* action = find_action(key);
    return action ? (g_spanish ? action->spanish : action->english) : key;
}

const char* action_detail(const char* key) {
    const Action* action = find_action(key);
    if (!action || !action->english_detail) {
        return nullptr;
    }
    return g_spanish ? action->spanish_detail : action->english_detail;
}
