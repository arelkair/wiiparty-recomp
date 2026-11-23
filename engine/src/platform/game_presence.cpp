#include "wp/game_presence.h"

#include <cstring>
#include <ctime>
#include <sstream>

#include "wp/discord_presence.h"
#include "wp/game.h"
#include "wp/log.h"
#include "wp/settings.h"

#ifndef WP_BUILD
#define WP_BUILD ""
#endif

namespace wp::presence {

namespace {

constexpr const char* kReleases = "https://github.com/arelkair/wiiparty-recomp/releases";
constexpr const char* kSource = "https://github.com/arelkair/wiiparty-recomp";
constexpr const char* kLargeImage = "logo";
constexpr const char* kLanguageCodes[game::kPresenceLanguages] = {"en", "es", "gl", "de", "fr", "it", "nl"};
constexpr const char* kDownload[game::kPresenceLanguages] = {"Get", "Descargar", "Descargar", "Herunterladen", "Télécharger", "Scarica", "Downloaden"};
constexpr const char* kSourceCode[game::kPresenceLanguages] = {"Source code", "Código fuente", "Código fonte", "Quellcode", "Code source", "Codice sorgente", "Broncode"};

std::string g_modules;
std::string g_published;
bool g_active = false;
int64_t g_started = 0;

size_t language_index() {
    std::string code = settings::text("system.language", "WP_LANGUAGE");
    if (code.empty() || code == "auto") {
        code = settings::text("system.interface_language", "WP_INTERFACE_LANGUAGE");
    }
    for (size_t i = 0; i < game::kPresenceLanguages; i++) {
        if (code == kLanguageCodes[i]) {
            return i;
        }
    }
    return 0;
}

const char* label_for(const std::string& modules, size_t language) {
    const game::Description& game = game::description();
    for (size_t i = 0; i < game.presence_rule_count; i++) {
        const game::PresenceRule& rule = game.presence_rules[i];
        std::istringstream names(modules);
        std::string name;
        while (names >> name) {
            if (name.rfind(rule.module_prefix, 0) == 0) {
                return rule.labels.text[language];
            }
        }
    }
    return game.presence_default.text[language];
}

void publish() {
    const game::Description& game = game::description();
    size_t language = language_index();
    discord::Activity activity;
    activity.details = std::string(game.title) + " Recomp";
    activity.state = label_for(g_modules, language);
    activity.large_image = kLargeImage;
    activity.large_text = activity.details + " " + WP_BUILD;
    activity.start = g_started;
    activity.first_label = std::string(kDownload[language]) + " " + activity.details;
    activity.first_url = kReleases;
    activity.second_label = kSourceCode[language];
    activity.second_url = kSource;
    std::string signature = activity.state + "|" + kLanguageCodes[language];
    if (signature == g_published) {
        return;
    }
    g_published = signature;
    log::write("discord", "presence: %s (%s)", activity.state.c_str(), kLanguageCodes[language]);
    discord::update(activity);
}

}

void modules_changed(const std::string& modules) {
    g_modules = modules;
    if (g_active) {
        publish();
    }
}

void poll() {
    static const settings::LiveFlag enabled("system.discord_presence", "WP_DISCORD_PRESENCE");
    static std::string shown_language;
    const char* application = game::description().discord_application_id;
    bool want = application && *application && enabled();
    if (want && !g_active) {
        g_started = static_cast<int64_t>(std::time(nullptr));
        discord::start(application);
        g_active = true;
        publish();
    } else if (!want && g_active) {
        discord::stop();
        g_active = false;
        g_published.clear();
    }
    if (g_active) {
        std::string current = kLanguageCodes[language_index()];
        if (current != shown_language) {
            shown_language = current;
            publish();
        }
    }
}

}
