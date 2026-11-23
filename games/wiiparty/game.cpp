#include "wp/game.h"

#include <iterator>

namespace wp::game {

namespace {

constexpr const char* kSidewaysModules[] = {
    "mg102", "mg104", "mg108", "mg109", "mg202", "mg210", "mg212", "mg213", "mg218", "mg221", "mg222", "mg223", "mg224",
    "mg302", "mg404", "mg407", "mg412", "mg415", "mg422", "mg428", "mg430", "mg431", "mg432", "mg433", "mg436", "mg437",
    "mg438", "mg440", "mg441", "mg445", "mg446", "mg503", "mg504", "mg505", "mg507", "mg508", "mg509",
};

#ifndef WP_DISCORD_APP_ID
#define WP_DISCORD_APP_ID ""
#endif

constexpr PresenceRule kPresenceRules[] = {
    {"mg", {{"Playing a minigame", "Jugando a un minijuego", "Xogando a un minixogo", "Spielt ein Minispiel", "Joue à un mini-jeu", "Gioca a un minigioco", "Speelt een minigame"}}},
    {"mr001", {{"Playing Board Game Island", "Jugando a Isla Aventura", "Xogando a Illa Aventura", "Spielt auf der Brettspielinsel", "Joue sur l'île des jeux de plateau", "Gioca sull'isola dei giochi da tavolo", "Speelt op het bordspeleiland"}}},
    {"inst", {{"Reading the minigame rules", "Leyendo las reglas del minijuego", "Lendo as regras do minixogo", "Liest die Minispiel-Regeln", "Lit les règles du mini-jeu", "Legge le regole del minigioco", "Leest de spelregels"}}},
    {"menu", {{"In the menus", "En los menús", "Nos menús", "In den Menüs", "Dans les menus", "Nei menu", "In de menu's"}}},
};

constexpr Description kDescription = {
    "Wii Party",
    "wiiparty",
    "games/wiiparty/extracted",
    "games/wiiparty/nand",
    "games/wiiparty/settings.ini",
    "games/wiiparty/textures",
    "boot",
    kSidewaysModules,
    std::size(kSidewaysModules),
    "inst",
    "mg",
    WP_DISCORD_APP_ID,
    {{"Starting the game", "Iniciando el juego", "Iniciando o xogo", "Spiel wird gestartet", "Démarrage du jeu", "Avvio del gioco", "Spel wordt gestart"}},
    kPresenceRules,
    std::size(kPresenceRules),
};

}

const Description& description() {
    return kDescription;
}

}
