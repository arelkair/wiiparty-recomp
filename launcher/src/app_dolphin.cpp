#include "app.h"

#include <ctime>
#include <fstream>
#include <iterator>

#include "layout.h"
#include "prefs.h"
#include "subprocess.h"
#include "texts.h"
#include "wp/save_backup.h"

namespace {

namespace fs = std::filesystem;

constexpr size_t kMiiCount = 100;
constexpr size_t kMiiSize = 0x4A;
constexpr size_t kMiiNameOffset = 2;
constexpr size_t kMiiNameSize = 20;

fs::path mii_database(const fs::path& wii) {
    return wii / "shared2" / "menu" / "FaceLib" / "RFL_DB.dat";
}

uint32_t title_low(const std::string& disc_id) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4 && i < disc_id.size(); i++) {
        value = (value << 8) | static_cast<uint8_t>(disc_id[i]);
    }
    return value;
}

int count_miis(const fs::path& database) {
    std::ifstream file(database, std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.size() < 4 + kMiiCount * kMiiSize || data.compare(0, 4, "RNOD") != 0) {
        return 0;
    }
    int count = 0;
    for (size_t i = 0; i < kMiiCount; i++) {
        size_t name = 4 + i * kMiiSize + kMiiNameOffset;
        for (size_t b = 0; b < kMiiNameSize; b++) {
            if (data[name + b] != 0) {
                count++;
                break;
            }
        }
    }
    return count;
}

std::vector<fs::path> dolphin_folders() {
    std::vector<fs::path> folders;
    auto add = [&](const std::string& base, std::initializer_list<const char*> parts) {
        if (base.empty()) {
            return;
        }
        fs::path folder = path_from(base);
        for (const char* part : parts) {
            folder /= part;
        }
        folders.push_back(folder);
    };
#ifdef _WIN32
    add(environment_value("APPDATA"), {"Dolphin Emulator"});
    add(environment_value("USERPROFILE"), {"Documents", "Dolphin Emulator"});
    add(environment_value("USERPROFILE"), {"OneDrive", "Documents", "Dolphin Emulator"});
#else
    add(environment_value("XDG_DATA_HOME"), {"dolphin-emu"});
    add(environment_value("HOME"), {".local", "share", "dolphin-emu"});
    add(environment_value("HOME"), {".var", "app", "org.DolphinEmu.dolphin-emu", "data", "dolphin-emu"});
    add(environment_value("HOME"), {".dolphin-emu"});
    add(environment_value("HOME"), {"Library", "Application Support", "Dolphin"});
#endif
    return folders;
}

fs::path wii_folder_of(const fs::path& chosen) {
    std::error_code error;
    if (fs::is_directory(chosen / "Wii" / "title", error) || fs::is_directory(chosen / "Wii" / "shared2", error)) {
        return chosen / "Wii";
    }
    if (fs::is_directory(chosen / "title", error) || fs::is_directory(chosen / "shared2", error)) {
        return chosen;
    }
    return {};
}

bool copy_folder(const fs::path& from, const fs::path& to, std::string& message) {
    std::error_code error;
    fs::create_directories(to, error);
    for (const auto& entry : fs::directory_iterator(from, error)) {
        if (entry.is_regular_file(error)) {
            fs::copy_file(entry.path(), to / entry.path().filename(), fs::copy_options::overwrite_existing, error);
            if (error) {
                message = error.message();
                return false;
            }
        }
    }
    if (error) {
        message = error.message();
        return false;
    }
    return true;
}

}

void App::scan_dolphin() {
    dolphin_ = DolphinFind{};
    std::vector<fs::path> candidates;
    std::string chosen = pref("dolphin_folder");
    if (!chosen.empty()) {
        candidates.push_back(path_from(chosen));
    }
    for (const fs::path& folder : dolphin_folders()) {
        candidates.push_back(folder);
    }
    fs::path save = wp::saves::save_relative(title_low(project_.disc_id()));
    for (const fs::path& candidate : candidates) {
        fs::path wii = wii_folder_of(candidate);
        if (wii.empty()) {
            continue;
        }
        std::error_code error;
        bool has_save = fs::exists(wii / save / "wiiparty.bin", error);
        int miis = count_miis(mii_database(wii));
        if (has_save || miis > 0) {
            dolphin_.wii = wii;
            dolphin_.has_save = has_save;
            dolphin_.miis = miis;
            break;
        }
    }
    dolphin_.scanned = true;
}

void App::import_dolphin() {
    const Texts& t = texts();
    std::string message;
    fs::path nand = project_.nand_folder();
    fs::path save = wp::saves::save_relative(title_low(project_.disc_id()));
    std::error_code error;
    fs::create_directories(nand, error);
    wp::saves::adopt_root_files(nand, save);
    bool ok = true;
    if (dolphin_.has_save) {
        int keep = std::atoi(value("saves.backups").c_str());
        wp::saves::Outcome outcome = wp::saves::back_up(nand, save, project_.backups_folder(), keep, std::time(nullptr));
        if (outcome.result == wp::saves::Result::Failed) {
            ok = false;
            message = outcome.message;
        } else {
            ok = copy_folder(dolphin_.wii / save, nand / save, message);
        }
    }
    if (ok && dolphin_.miis > 0) {
        fs::path target = mii_database(nand);
        fs::create_directories(target.parent_path(), error);
        if (fs::exists(target, error)) {
            fs::create_directories(project_.backups_folder(), error);
            char stamp[32];
            std::time_t now = std::time(nullptr);
            std::strftime(stamp, sizeof stamp, "RFL_DB-%Y-%m-%d_%H-%M-%S.dat", std::localtime(&now));
            fs::copy_file(target, project_.backups_folder() / stamp, fs::copy_options::overwrite_existing, error);
        }
        fs::copy_file(mii_database(dolphin_.wii), target, fs::copy_options::overwrite_existing, error);
        if (error) {
            ok = false;
            message = error.message();
        }
    }
    saves_message_ = ok ? std::string(t.dolphin_done) : format(t.dolphin_failed, message);
    refresh_backups();
}

void App::dolphin_section(float width) {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    {
        std::lock_guard<std::mutex> lock(picked_mutex_);
        if (!picked_dolphin_.empty()) {
            set_pref("dolphin_folder", picked_dolphin_);
            picked_dolphin_.clear();
            dolphin_.scanned = false;
        }
    }
    if (!dolphin_.scanned) {
        scan_dolphin();
    }
    ui::text(t.dolphin_title, Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    Card card(width);
    std::string where = utf8_of(dolphin_.wii.parent_path());
    std::string miis = std::to_string(dolphin_.miis);
    std::string detail = dolphin_.wii.empty()          ? std::string(t.dolphin_none)
                         : dolphin_.has_save && dolphin_.miis > 0 ? format(t.dolphin_both, where, miis)
                         : dolphin_.has_save                   ? format(t.dolphin_save, where)
                                                               : format(t.dolphin_miis, where, miis);
    row(width, t.dolphin_label, detail.c_str(), 250.0f, 36.0f, [&] {
        if (ui::button(t.dolphin_choose, Kind::Secondary, 120.0f)) {
            std::string start = dolphin_.wii.empty() ? std::string() : utf8_of(dolphin_.wii.parent_path());
            SDL_ShowOpenFolderDialog(
                [](void* self, const char* const* files, int) {
                    if (files && files[0]) {
                        App* app = static_cast<App*>(self);
                        std::lock_guard<std::mutex> lock(app->picked_mutex_);
                        app->picked_dolphin_ = files[0];
                    }
                    wake_main_loop();
                },
                this, window_, start.empty() ? nullptr : start.c_str(), false);
        }
        ImGui::SameLine(0, px(10));
        if (ui::button(t.dolphin_import, Kind::Primary, 120.0f, !dolphin_.wii.empty())) {
            import_dolphin();
        }
    });
    card.end();
    ui::gap(28);
}
