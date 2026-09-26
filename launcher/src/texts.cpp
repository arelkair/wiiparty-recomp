#include "texts.h"

#include <QLocale>

namespace {

Texts english() {
    Texts t;
    t.play = "Play";
    t.build = "Build";
    t.settings = "Settings";
    t.play_heading = "Wii Party";
    t.status_ready = "Ready to play.";
    t.status_not_built = "The game has not been built yet. Build it from your own copy of the disc first.";
    t.status_no_disc = "No extracted disc was found. Choose your copy of Wii Party on the Build page.";
    t.play_button = "Play";
    t.play_hint = "F11 switches full screen while playing. F10 saves a screenshot in the screenshots folder. F12 saves a capture of the frame for debugging.";
    t.build_heading = "Build";
    t.build_intro = "Translates the game from your own copy of the disc into a native program. Nothing is downloaded from the game, and nothing leaves your computer. The first build takes about 15 minutes on a 6-core processor.";
    t.tools = "Tools";
    t.found = "Found";
    t.missing = "Missing";
    t.disc = "Disc";
    t.disc_none = "No disc chosen";
    t.disc_extracted = "Already extracted";
    t.choose_disc = "Choose disc...";
    t.choose_disc_title = "Choose your copy of Wii Party";
    t.disc_filter = "Wii disc images (*.iso *.wbfs *.rvz *.ciso *.wia *.gcm)";
    t.start_build = "Build";
    t.cancel = "Cancel";
    t.step_extract = "Extract the disc";
    t.step_sdl = "Download SDL3 for gamepads";
    t.step_unpack = "Unpack the code modules";
    t.step_modules = "Translate the code modules";
    t.step_dol = "Translate the main executable";
    t.step_links = "Link the modules to the main executable";
    t.step_dsp = "Translate the audio microcode";
    t.step_configure = "Configure the build";
    t.step_compile = "Compile";
    t.step_waiting = "Waiting";
    t.step_running = "Running";
    t.step_done = "Done";
    t.step_skipped = "Skipped";
    t.step_failed = "Failed";
    t.build_finished = "The build finished. The game is ready to play.";
    t.build_failed = "The build stopped at a failed step. The log above shows the error.";
    t.build_cancelled = "The build was cancelled.";
    t.missing_tool = "Missing tool: %1";
    t.settings_heading = "Settings";
    t.settings_intro = "PC improvements, kept apart from the translated game. Changes apply the next time the game starts.";
    t.group_video = "Video";
    t.group_input = "Input";
    t.group_system = "System";
    t.group_audio = "Audio";
    t.scale = "Internal resolution";
    t.scale_detail = "Renders the game at a multiple of the console's resolution.";
    t.scale_native = "Native";
    t.fullscreen = "Full screen";
    t.fullscreen_detail = "Starts in borderless full screen. F11 also switches it while playing.";
    t.copy_filter = "Anti-flicker filter";
    t.copy_filter_detail = "The soft vertical filter the game uses on the console. Turn it off for a sharper picture.";
    t.custom_textures = "Custom textures";
    t.custom_textures_detail = "Uses the replacement textures in games/wiiparty/textures/load, with Dolphin's file names.";
    t.dump_textures = "Dump textures";
    t.dump_textures_detail = "Saves each texture the game uses to games/wiiparty/textures/dump, for making texture packs.";
    t.gamepads = "Gamepads";
    t.gamepads_detail = "Up to four gamepads act as Wii Remotes 1 to 4.";
    t.auto_grip = "Automatic grip";
    t.auto_grip_detail = "Holds the emulated Wii Remote sideways in the minigames that ask for it.";
    t.wake_on_mouse = "Reconnect with the mouse";
    t.wake_on_mouse_detail = "Moving the mouse reconnects a Wii Remote the game disconnected for inactivity.";
    t.hide_cursor = "Hide the Windows cursor";
    t.hide_cursor_detail = "Hides the system cursor over the game; the game draws its own pointer.";
    t.language = "Console language";
    t.language_detail = "The language of the virtual console, which the game follows.";
    t.language_auto = "Same as the system";
    t.pal60 = "60 Hz (PAL60)";
    t.pal60_detail = "The console's own PAL60 setting. Off runs the game at 50 Hz.";
    t.skip_notices = "Skip the strap notice";
    t.skip_notices_detail = "Presses A on the Wii Remote strap notice at start, as a player would.";
    t.mute = "Mute";
    t.mute_detail = "Silences the game's sound.";
    t.group_saves = "Saves";
    t.backups = "Save backups";
    t.backups_detail = "How many copies of the save to keep. A copy is made at every start when the save changed, and at most once a minute after the game saves.";
    t.backups_off = "Off";
    t.saves = "Saves";
    t.saves_heading = "Saves";
    t.saves_intro = "Copies of your progress, made automatically. Restoring one replaces the current save, which is first kept as a new copy.";
    t.saves_backups = "Backups";
    t.saves_empty = "No backups yet. The first one is made the next time the game starts with a save.";
    t.saves_disabled = "Automatic backups are off. Turn them on in Settings.";
    t.open_folder = "Open folder";
    t.restore = "Restore";
    t.backup_size = "%1 KB";
    t.restore_title = "Restore a backup";
    t.restore_question = "Replace the current save with the backup from %1?\n\nClose the game first. The current save is kept as a new backup.";
    t.restore_done = "The backup from %1 is now the save.";
    t.restore_unchanged = "The save already matches the backup from %1.";
    t.restore_failed = "The backup could not be restored: %1";
    t.game = "Game";
    t.install = "Install";
    t.install_intro = "Installing translates the game from your own copy of the disc into a native program. Missing tools are downloaded first from their official sites; nothing from the game is downloaded and nothing leaves your computer. It takes about 15 minutes on a 6-core processor.";
    t.choose_disc_first = "Choose your copy of Wii Party first.";
    t.checking_tools = "Checking the tools...";
    t.tool_ready = "%1: %2, ready";
    t.tool_to_download = "%1: missing or not compatible, it will be downloaded";
    t.step_tool = "Find %1";
    t.tool_unavailable = "%1 is missing. Install it with your system's package manager and try again.";
    t.step_download = "Download %1";
    t.step_verify = "Check %1";
    t.step_install = "Unpack %1";
    t.step_finish = "Finish %1";
    t.hash_mismatch = "%1 does not match its published checksum and was deleted. Try again.";
    t.project_missing = "The project folder was not found. Start the launcher from inside the wiiparty-recomp folder.";
    t.start_failed = "The game could not be started.";
    t.controls = "Controls";
    t.controls_heading = "Controls";
    t.controls_intro = "Keyboard and mouse keys of the emulated Wii Remote 1. Click a binding and press a key, click a mouse button or turn the wheel on it to add it. F11 and F12 are reserved. Changes apply the next time the game starts.";
    t.controls_reset = "Reset to defaults";
    t.keys_clear = "Clear";
    t.keys_none = "Not set";
    t.keys_press = "Press a key, click or scroll here (Esc cancels)";
    t.keys_reserved = "F11 and F12 are reserved, press another key";
    t.actions = {
        {"a", "A button"},
        {"b", "B button"},
        {"one", "1 button"},
        {"two", "2 button"},
        {"plus", "+ button"},
        {"minus", "- button"},
        {"home", "HOME button"},
        {"up", "D-pad up"},
        {"down", "D-pad down"},
        {"left", "D-pad left"},
        {"right", "D-pad right"},
        {"shake", "Shake"},
        {"swing_up", "Swing up"},
        {"swing_down", "Swing down"},
        {"tilt_left", "Tilt left"},
        {"tilt_right", "Tilt right"},
        {"tilt_up", "Tilt up"},
        {"tilt_down", "Tilt down"},
        {"grip", "Swap upright and sideways grip"},
        {"screenshot", "Screenshot"},
    };
    t.action_details = {
        {"up", "Left on screen when the remote is held sideways."},
        {"down", "Right on screen when the remote is held sideways."},
        {"left", "Down on screen when the remote is held sideways."},
        {"right", "Up on screen when the remote is held sideways."},
        {"swing_up", "The mouse wheel gives a short swing."},
        {"swing_down", "The mouse wheel gives a short swing."},
        {"screenshot", "Saves the picture of the game as PNG in the screenshots folder."},
    };
    return t;
}

Texts spanish() {
    Texts t = english();
    t.play = "Jugar";
    t.build = "Compilar";
    t.settings = "Opciones";
    t.status_ready = "Listo para jugar.";
    t.status_not_built = "El juego aún no está compilado. Compílalo primero a partir de tu propia copia del disco.";
    t.status_no_disc = "No se ha encontrado el disco extraído. Elige tu copia de Wii Party en la página Compilar.";
    t.play_button = "Jugar";
    t.play_hint = "F11 cambia a pantalla completa durante la partida. F10 guarda una captura de pantalla en la carpeta screenshots. F12 guarda una captura del fotograma para depuración.";
    t.build_heading = "Compilar";
    t.build_intro = "Traduce el juego desde tu propia copia del disco a un programa nativo. No se descarga nada del juego y nada sale de tu ordenador. La primera compilación tarda unos 15 minutos con un procesador de 6 núcleos.";
    t.tools = "Herramientas";
    t.found = "Encontrada";
    t.missing = "Falta";
    t.disc = "Disco";
    t.disc_none = "Ningún disco elegido";
    t.disc_extracted = "Ya extraído";
    t.choose_disc = "Elegir disco...";
    t.choose_disc_title = "Elige tu copia de Wii Party";
    t.disc_filter = "Imágenes de disco de Wii (*.iso *.wbfs *.rvz *.ciso *.wia *.gcm)";
    t.start_build = "Compilar";
    t.cancel = "Cancelar";
    t.step_extract = "Extraer el disco";
    t.step_sdl = "Descargar SDL3 para los mandos";
    t.step_unpack = "Descomprimir los módulos de código";
    t.step_modules = "Traducir los módulos de código";
    t.step_dol = "Traducir el ejecutable principal";
    t.step_links = "Enlazar los módulos con el ejecutable principal";
    t.step_dsp = "Traducir el microcódigo de audio";
    t.step_configure = "Preparar la compilación";
    t.step_compile = "Compilar";
    t.step_waiting = "En espera";
    t.step_running = "En curso";
    t.step_done = "Hecho";
    t.step_skipped = "Omitido";
    t.step_failed = "Error";
    t.build_finished = "La compilación ha terminado. El juego está listo.";
    t.build_failed = "La compilación se ha detenido en un paso con error. El registro de arriba muestra el fallo.";
    t.build_cancelled = "La compilación se ha cancelado.";
    t.missing_tool = "Falta la herramienta: %1";
    t.settings_heading = "Opciones";
    t.settings_intro = "Mejoras de PC, separadas del juego traducido. Los cambios se aplican la próxima vez que se abra el juego.";
    t.group_video = "Vídeo";
    t.group_input = "Entrada";
    t.group_system = "Sistema";
    t.group_audio = "Sonido";
    t.scale = "Resolución interna";
    t.scale_detail = "Dibuja el juego a un múltiplo de la resolución de la consola.";
    t.scale_native = "Nativa";
    t.fullscreen = "Pantalla completa";
    t.fullscreen_detail = "Empieza en pantalla completa sin bordes. F11 también la cambia durante la partida.";
    t.copy_filter = "Filtro anti-parpadeo";
    t.copy_filter_detail = "El suave filtro vertical que usa el juego en la consola. Desactívalo para una imagen más nítida.";
    t.custom_textures = "Texturas personalizadas";
    t.custom_textures_detail = "Usa las texturas de reemplazo de games/wiiparty/textures/load, con los nombres de archivo de Dolphin.";
    t.dump_textures = "Volcar texturas";
    t.dump_textures_detail = "Guarda cada textura que usa el juego en games/wiiparty/textures/dump, para crear packs de texturas.";
    t.gamepads = "Mandos";
    t.gamepads_detail = "Hasta cuatro mandos funcionan como los Wii Remote 1 a 4.";
    t.auto_grip = "Agarre automático";
    t.auto_grip_detail = "Sujeta el Wii Remote emulado en horizontal en los minijuegos que lo piden.";
    t.wake_on_mouse = "Reconectar con el ratón";
    t.wake_on_mouse_detail = "Mover el ratón reconecta un Wii Remote que el juego desconectó por inactividad.";
    t.hide_cursor = "Ocultar el cursor de Windows";
    t.hide_cursor_detail = "Oculta el cursor del sistema sobre el juego; el juego dibuja su propio puntero.";
    t.language = "Idioma de la consola";
    t.language_detail = "El idioma de la consola virtual, que el juego sigue.";
    t.language_auto = "El del sistema";
    t.pal60_detail = "El ajuste PAL60 de la propia consola. Desactivado, el juego va a 50 Hz.";
    t.skip_notices = "Saltar el aviso de la correa";
    t.skip_notices_detail = "Pulsa A en el aviso de la correa del Wii Remote al empezar, como haría un jugador.";
    t.mute = "Silenciar";
    t.mute_detail = "Quita el sonido del juego.";
    t.group_saves = "Partidas";
    t.backups = "Copias de seguridad";
    t.backups_detail = "Cuántas copias de la partida guardar. Se hace una copia al abrir el juego si la partida ha cambiado, y como mucho una por minuto después de que el juego guarde.";
    t.backups_off = "Desactivadas";
    t.saves = "Partidas";
    t.saves_heading = "Partidas";
    t.saves_intro = "Copias de tu progreso, hechas automáticamente. Restaurar una sustituye la partida actual, que antes se guarda como una copia nueva.";
    t.saves_backups = "Copias de seguridad";
    t.saves_empty = "Aún no hay copias. La primera se hará la próxima vez que el juego se abra con una partida guardada.";
    t.saves_disabled = "Las copias automáticas están desactivadas. Actívalas en Opciones.";
    t.open_folder = "Abrir carpeta";
    t.restore = "Restaurar";
    t.restore_title = "Restaurar una copia";
    t.restore_question = "¿Sustituir la partida actual por la copia del %1?\n\nCierra antes el juego. La partida actual se guarda como una copia nueva.";
    t.restore_done = "La copia del %1 es ahora la partida.";
    t.restore_unchanged = "La partida ya coincide con la copia del %1.";
    t.restore_failed = "No se ha podido restaurar la copia: %1";
    t.game = "Juego";
    t.install = "Instalar";
    t.install_intro = "Instalar traduce el juego desde tu propia copia del disco a un programa nativo. Las herramientas que falten se descargan antes desde sus webs oficiales; no se descarga nada del juego y nada sale de tu ordenador. Tarda unos 15 minutos con un procesador de 6 núcleos.";
    t.choose_disc_first = "Elige primero tu copia de Wii Party.";
    t.checking_tools = "Comprobando las herramientas...";
    t.tool_ready = "%1: %2, lista";
    t.tool_to_download = "%1: falta o no es compatible, se descargará";
    t.step_tool = "Buscar %1";
    t.tool_unavailable = "Falta %1. Instálalo con el gestor de paquetes de tu sistema y vuelve a intentarlo.";
    t.step_download = "Descargar %1";
    t.step_verify = "Comprobar %1";
    t.step_install = "Descomprimir %1";
    t.step_finish = "Terminar %1";
    t.hash_mismatch = "%1 no coincide con su huella publicada y se ha borrado. Vuelve a intentarlo.";
    t.project_missing = "No se ha encontrado la carpeta del proyecto. Abre el lanzador desde dentro de la carpeta wiiparty-recomp.";
    t.start_failed = "No se ha podido abrir el juego.";
    t.controls = "Controles";
    t.controls_heading = "Controles";
    t.controls_intro = "Teclas y botones del ratón del Wii Remote 1 emulado. Haz clic en una asignación y pulsa una tecla, haz clic con un botón del ratón o gira la rueda sobre ella para añadirla. F11 y F12 están reservadas. Los cambios se aplican la próxima vez que se abra el juego.";
    t.controls_reset = "Restablecer valores predeterminados";
    t.keys_clear = "Borrar";
    t.keys_none = "Sin asignar";
    t.keys_press = "Pulsa una tecla, haz clic o gira la rueda aquí (Esc cancela)";
    t.keys_reserved = "F11 y F12 están reservadas, pulsa otra tecla";
    t.actions = {
        {"a", "Botón A"},
        {"b", "Botón B"},
        {"one", "Botón 1"},
        {"two", "Botón 2"},
        {"plus", "Botón +"},
        {"minus", "Botón -"},
        {"home", "Botón HOME"},
        {"up", "Cruceta arriba"},
        {"down", "Cruceta abajo"},
        {"left", "Cruceta izquierda"},
        {"right", "Cruceta derecha"},
        {"shake", "Agitar"},
        {"swing_up", "Mover hacia arriba"},
        {"swing_down", "Mover hacia abajo"},
        {"tilt_left", "Inclinar a la izquierda"},
        {"tilt_right", "Inclinar a la derecha"},
        {"tilt_up", "Inclinar hacia arriba"},
        {"tilt_down", "Inclinar hacia abajo"},
        {"grip", "Cambiar entre agarre vertical y horizontal"},
        {"screenshot", "Captura de pantalla"},
    };
    t.action_details = {
        {"up", "Izquierda en pantalla con el mando en horizontal."},
        {"down", "Derecha en pantalla con el mando en horizontal."},
        {"left", "Abajo en pantalla con el mando en horizontal."},
        {"right", "Arriba en pantalla con el mando en horizontal."},
        {"swing_up", "La rueda del ratón da un movimiento corto."},
        {"swing_down", "La rueda del ratón da un movimiento corto."},
        {"screenshot", "Guarda la imagen del juego como PNG en la carpeta screenshots."},
    };
    return t;
}

}

const Texts& texts() {
    static const Texts value = QLocale::system().language() == QLocale::Spanish ? spanish() : english();
    return value;
}
