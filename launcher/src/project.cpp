#include "project.h"

#include <SDL3/SDL.h>

#include "embedded.h"
#include "payload.h"
#include "subprocess.h"

namespace {

bool contains_game(const std::filesystem::path& folder) {
    std::error_code error;
    return std::filesystem::exists(folder / "games" / kGame / "game.toml", error);
}

bool search_up(std::filesystem::path folder, std::filesystem::path& root) {
    while (!folder.empty()) {
        if (contains_game(folder)) {
            root = folder;
            return true;
        }
        std::filesystem::path parent = folder.parent_path();
        if (parent == folder) {
            break;
        }
        folder = parent;
    }
    return false;
}

}

std::filesystem::path Project::game_folder() const {
    return root / "games" / kGame;
}

std::filesystem::path Project::extracted_folder() const {
    return game_folder() / "extracted";
}

std::filesystem::path Project::nand_folder() const {
    return game_folder() / "nand";
}

std::filesystem::path Project::backups_folder() const {
    return game_folder() / "backups";
}

std::filesystem::path Project::executable() const {
#ifdef _WIN32
    return root / "build" / "out" / (std::string(kGame) + ".exe");
#else
    return root / "build" / "out" / kGame;
#endif
}

std::string Project::settings_file() const {
    return std::string("games/") + kGame + "/settings.ini";
}

bool Project::extracted() const {
    std::error_code error;
    return std::filesystem::exists(extracted_folder() / "sys" / "main.dol", error);
}

bool Project::built() const {
    std::error_code error;
    return std::filesystem::exists(executable(), error);
}

bool Project::outdated() const {
    return packaged && built() && stored_revision(root) != source_revision();
}

double Project::needed_gigabytes() const {
    if (built()) {
        return 1.0;
    }
    return extracted() ? 4.0 : 5.0;
}

double Project::free_gigabytes() const {
    std::error_code error;
    std::filesystem::path folder = root;
    while (!folder.empty() && !std::filesystem::exists(folder, error)) {
        std::filesystem::path parent = folder.parent_path();
        if (parent == folder) {
            break;
        }
        folder = parent;
    }
    std::filesystem::space_info info = std::filesystem::space(folder, error);
    return error ? -1.0 : static_cast<double>(info.available) / (1024.0 * 1024.0 * 1024.0);
}

bool find_checkout(std::filesystem::path& root) {
    const char* base = SDL_GetBasePath();
    if (base && search_up(path_from(base), root)) {
        return true;
    }
    std::error_code error;
    return search_up(std::filesystem::current_path(error), root);
}

std::filesystem::path default_install_folder() {
#ifdef _WIN32
    std::string local = environment_value("LOCALAPPDATA");
    if (!local.empty()) {
        return path_from(local) / kInstallFolderName;
    }
#endif
    char* data = SDL_GetPrefPath(nullptr, kInstallFolderName);
    std::filesystem::path folder = data ? path_from(data) : std::filesystem::path(kInstallFolderName);
    SDL_free(data);
    return folder;
}
