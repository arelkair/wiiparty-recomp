#include "app.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "layout.h"
#include "payload.h"
#include "prefs.h"
#include "subprocess.h"
#include "texts.h"
#include "wp/options.h"

#ifdef _WIN32
#include <windows.h>
#include <dxgi.h>
#endif

namespace {

namespace fs = std::filesystem;

constexpr int kConsoleHeight = 528;
constexpr int kMaxScale = 6;
constexpr float kSmoothFps = 58.0f;

std::string system_name() {
#ifdef _WIN32
    using Version = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto get = ntdll ? reinterpret_cast<Version>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion"))) : nullptr;
    OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (get && get(&info) == 0) {
        std::string name = info.dwMajorVersion == 10 && info.dwBuildNumber >= 22000 ? "Windows 11" : "Windows " + std::to_string(info.dwMajorVersion);
        return name + " (build " + std::to_string(info.dwBuildNumber) + ")";
    }
    return "Windows";
#else
    std::ifstream file("/etc/os-release");
    for (std::string line; std::getline(file, line);) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            std::string value = line.substr(12);
            value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
            return value;
        }
    }
    return "Linux";
#endif
}

struct Graphics {
    std::string name;
    uint64_t memory = 0;
    bool integrated = false;
};

std::vector<Graphics> graphics_cards(const fs::path& game_log) {
    std::vector<Graphics> cards;
#ifdef _WIN32
    (void)game_log;
    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                Graphics card;
                card.name = utf8_of(fs::path(desc.Description));
                card.memory = desc.DedicatedVideoMemory;
                card.integrated = desc.DedicatedVideoMemory < 512ull * 1024 * 1024;
                cards.push_back(card);
            }
            adapter->Release();
        }
        factory->Release();
    }
#else
    std::ifstream file(game_log);
    for (std::string line; std::getline(file, line);) {
        size_t found = line.find("OpenGL renderer: ");
        if (found != std::string::npos) {
            Graphics card;
            card.name = line.substr(found + 17);
            std::string lower = card.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            card.integrated = lower.find("intel") != std::string::npos || lower.find("llvmpipe") != std::string::npos ||
                              lower.find("softpipe") != std::string::npos || lower.find("vc4") != std::string::npos;
            cards = {card};
        }
    }
#endif
    return cards;
}

struct Session {
    bool found = false;
    float average = 0.0f;
    float lowest = 0.0f;
};

Session last_session(const fs::path& game_log) {
    Session session;
    std::ifstream file(game_log);
    float total = 0.0f;
    int count = 0;
    for (std::string line; std::getline(file, line);) {
        size_t found = line.find(" fps, ");
        if (found == std::string::npos) {
            continue;
        }
        size_t start = line.find_last_of(' ', found - 1);
        float fps = std::strtof(line.c_str() + (start == std::string::npos ? 0 : start + 1), nullptr);
        if (fps <= 0.0f) {
            continue;
        }
        if (count > 2) {
            session.lowest = session.found ? std::min(session.lowest, fps) : fps;
            session.found = true;
            total += fps;
        }
        count++;
    }
    if (session.found) {
        session.average = total / static_cast<float>(count - 3);
    }
    return session;
}

std::string one_decimal(float value) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%.1f", value);
    return buffer;
}

void copy_tree(const fs::path& from, const fs::path& to, uintmax_t& done, uintmax_t total, std::atomic<int>& percent, std::error_code& error) {
    fs::create_directories(to, error);
    for (const auto& entry : fs::directory_iterator(from, error)) {
        if (error) {
            return;
        }
        fs::path target = to / entry.path().filename();
        if (entry.is_directory(error)) {
            copy_tree(entry.path(), target, done, total, percent, error);
        } else {
            fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, error);
            done += entry.file_size(error);
            percent = total ? static_cast<int>(done * 100 / total) : 100;
        }
        if (error) {
            return;
        }
    }
}

uintmax_t tree_size(const fs::path& folder) {
    uintmax_t total = 0;
    std::error_code error;
    for (const auto& entry : fs::recursive_directory_iterator(folder, error)) {
        if (entry.is_regular_file(error)) {
            total += entry.file_size(error);
        }
    }
    return total;
}

}

int App::recommended_scale(bool& slowed) {
    slowed = false;
    SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display ? display : SDL_GetPrimaryDisplay());
    int height = mode ? mode->h : 1080;
    int scale = std::clamp(static_cast<int>(std::lround(static_cast<double>(height) / kConsoleHeight)), 1, kMaxScale);
    std::vector<Graphics> cards = graphics_cards(project_.root / "logs" / "game.log");
    bool integrated = !cards.empty() && std::all_of(cards.begin(), cards.end(), [](const Graphics& card) { return card.integrated; });
    if (integrated) {
        scale = std::min(scale, 2);
    }
    Session session = last_session(project_.root / "logs" / "game.log");
    int current = std::max(1, std::atoi(value("video.scale").c_str()));
    if (session.found && session.average < kSmoothFps && current <= scale) {
        scale = std::max(1, current - 1);
        slowed = true;
    }
    return scale;
}

std::vector<std::pair<std::string, std::string>> App::diagnostics() {
    const Texts& t = texts();
    std::vector<std::pair<std::string, std::string>> lines;
    lines.emplace_back(t.diag_launcher, std::string(WP_LAUNCHER_VERSION) + " (" + WP_LAUNCHER_BUILD + ")");
    std::string id = project_.disc_id();
    std::string revision = stored_revision(project_.root);
    lines.emplace_back(t.diag_game, project_.built() && !id.empty() ? id + (revision.empty() ? "" : ", " + revision) : std::string(t.diag_game_missing));
    lines.emplace_back(t.diag_system, system_name());
    lines.emplace_back(t.diag_processor, format(t.diag_processor_value, std::to_string(SDL_GetNumLogicalCPUCores()),
                                                one_decimal(static_cast<float>(SDL_GetSystemRAM()) / 1024.0f)));
    std::string graphics;
    for (const Graphics& card : graphics_cards(project_.root / "logs" / "game.log")) {
        if (!graphics.empty()) {
            graphics += "; ";
        }
        graphics += card.name;
        if (card.memory) {
            graphics += " (" + one_decimal(static_cast<float>(card.memory) / (1024.0f * 1024.0f * 1024.0f)) + " GB)";
        }
    }
    lines.emplace_back(t.diag_graphics, graphics.empty() ? std::string(t.diag_graphics_unknown) : graphics);
    SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display ? display : SDL_GetPrimaryDisplay());
    if (mode) {
        lines.emplace_back(t.diag_display, std::to_string(mode->w) + "x" + std::to_string(mode->h) + ", " + one_decimal(mode->refresh_rate) + " Hz");
    }
    int count = 0;
    SDL_JoystickID* pads = gamepads(count);
    std::string names;
    for (int i = 0; i < count; i++) {
        const char* name = SDL_GetGamepadNameForID(pads[i]);
        names += (names.empty() ? "" : "; ") + std::string(name ? name : "?");
    }
    SDL_free(pads);
    lines.emplace_back(t.diag_gamepads, names.empty() ? std::string(t.diag_none) : names);
    double free = project_.free_gigabytes();
    lines.emplace_back(t.diag_folder, format(t.diag_free, utf8_of(project_.root), one_decimal(static_cast<float>(std::max(free, 0.0)))));
    Session session = last_session(project_.root / "logs" / "game.log");
    lines.emplace_back(t.diag_session, session.found ? format(t.diag_session_value, one_decimal(session.average), one_decimal(session.lowest))
                                                     : std::string(t.diag_session_none));
    return lines;
}

void App::toggle_portable(bool on) {
    fs::path beside = launcher_path().parent_path();
    std::error_code error;
    set_pref("root", utf8_of(project_.root));
    if (on) {
        std::ofstream(beside / "portable.txt").close();
        export_prefs(beside / "launcher.ini");
    } else {
        export_prefs(installed_prefs_file());
        fs::remove(beside / "portable.txt", error);
    }
    start_detached(utf8_of(launcher_path()), beside);
    quit_ = true;
}

void App::move_next_to_launcher() {
    fs::path from = project_.root;
    fs::path to = portable_folder() / "game";
    task_state_ = 1;
    task_percent_ = 0;
    std::error_code error;
    fs::current_path(portable_folder(), error);
    if (task_thread_.joinable()) {
        task_thread_.join();
    }
    task_thread_ = std::thread([this, from, to] {
        std::error_code error;
        fs::rename(from, to, error);
        if (error) {
            error.clear();
            uintmax_t done = 0;
            copy_tree(from, to, done, tree_size(from), task_percent_, error);
            if (!error) {
                fs::remove_all(from, error);
            }
        }
        std::lock_guard<std::mutex> lock(task_mutex_);
        task_error_ = error ? error.message() : std::string();
        task_state_ = error ? 4 : 3;
        wake_main_loop();
    });
}

void App::uninstall(bool keep_data) {
    fs::path root = project_.root;
    task_state_ = 2;
    std::error_code error;
    fs::current_path(root.parent_path(), error);
    if (task_thread_.joinable()) {
        task_thread_.join();
    }
    task_thread_ = std::thread([this, root, keep_data] {
        std::error_code error;
        if (!keep_data) {
            fs::remove_all(root, error);
        } else {
            const fs::path kept[] = {fs::path("games") / "wiiparty" / "nand", fs::path("games") / "wiiparty" / "backups",
                                     fs::path("games") / "wiiparty" / "settings.ini", fs::path("games") / "wiiparty" / "textures",
                                     fs::path("screenshots")};
            fs::path games = root / "games";
            fs::path game = games / "wiiparty";
            for (const auto& entry : fs::directory_iterator(root, error)) {
                if (entry.path() != games && entry.path().filename() != "screenshots") {
                    fs::remove_all(entry.path(), error);
                }
            }
            for (const auto& entry : fs::directory_iterator(games, error)) {
                if (entry.path() != game) {
                    fs::remove_all(entry.path(), error);
                }
            }
            for (const auto& entry : fs::directory_iterator(game, error)) {
                bool keep = std::any_of(std::begin(kept), std::end(kept), [&](const fs::path& path) { return entry.path() == root / path; });
                if (!keep) {
                    fs::remove_all(entry.path(), error);
                }
            }
        }
        std::lock_guard<std::mutex> lock(task_mutex_);
        task_error_ = error ? error.message() : std::string();
        task_state_ = error ? 4 : 5;
        wake_main_loop();
    });
}

void App::uninstall_modal() {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    if (uninstall_confirm_ && !ImGui::IsPopupOpen("uninstall")) {
        ImGui::OpenPopup("uninstall");
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(px(480), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(24), px(22)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, px(16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(16));
    if (ImGui::BeginPopupModal("uninstall", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        float inner = px(480) - px(48);
        ui::text(t.uninstall_label, Font::Semibold, 17.0f, p.text);
        ui::gap(8);
        ui::text(format(t.uninstall_question, utf8_of(project_.root)).c_str(), Font::Regular, ui::size::kBody, p.secondary, inner);
        ui::gap(16);
        ImVec2 at = ImGui::GetCursorScreenPos();
        ui::text(t.uninstall_keep, Font::Regular, ui::size::kBody, p.text, inner - px(60));
        ImVec2 after = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(at.x + inner - px(42), at.y - px(2)));
        if (ui::toggle("keep", uninstall_keep_)) {
            uninstall_keep_ = !uninstall_keep_;
        }
        ImGui::SetCursorScreenPos(ImVec2(at.x, std::max(after.y, at.y + px(24)) + px(24)));
        at = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(at.x + inner - px(130) - px(10) - px(110), at.y));
        if (ui::button(t.cancel, Kind::Ghost, 110.0f)) {
            uninstall_confirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetCursorScreenPos(ImVec2(at.x + inner - px(130), at.y));
        if (ui::button(t.uninstall_confirm, Kind::Primary, 130.0f)) {
            uninstall_confirm_ = false;
            uninstall(uninstall_keep_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
}

void App::system_page(float width) {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    heading(t.page_system, t.system_intro, width);
    int state = task_state_;
    if (state == 3 || state == 5) {
        task_state_ = 0;
        if (task_thread_.joinable()) {
            task_thread_.join();
        }
        if (state == 3) {
            set_pref("root", "");
            use_root(portable_folder() / "game");
            system_message_ = t.portable_moved;
        } else {
            use_root(project_.root);
            system_message_ = t.uninstalled;
        }
    } else if (state == 4) {
        task_state_ = 0;
        if (task_thread_.joinable()) {
            task_thread_.join();
        }
        std::lock_guard<std::mutex> lock(task_mutex_);
        system_message_ = format(t.task_failed, task_error_);
    }

    if (!system_ready_) {
        system_lines_ = diagnostics();
        system_scale_ = recommended_scale(system_slowed_);
        system_ready_ = true;
    }
    ui::text(t.diag_title, Font::Semibold, ui::size::kDetail, p.secondary);
    ui::gap(10);
    const std::vector<std::pair<std::string, std::string>>& lines = system_lines_;
    Card diagnostics_card(width);
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) {
            inset_separator(width);
        }
        ImGui::PushID(static_cast<int>(i));
        row(width, lines[i].first.c_str(), lines[i].second.c_str(), 0.0f, 0.0f, [] {});
        ImGui::PopID();
    }
    inset_separator(width);
    row(width, diagnostics_copied_ ? t.diag_copied : "", "", 110.0f, 36.0f, [&] {
        if (ui::button(t.diag_copy, Kind::Secondary, 110.0f)) {
            std::string text;
            for (const auto& [label, line] : lines) {
                text += label + ": " + line + "\n";
            }
            SDL_SetClipboardText(text.c_str());
            diagnostics_copied_ = true;
        }
    });
    diagnostics_card.end();
    ui::gap(28);

    Card tools(width);
    bool slowed = system_slowed_;
    int scale = system_scale_;
    int current = std::max(1, std::atoi(value("video.scale").c_str()));
    SDL_DisplayID display = SDL_GetDisplayForWindow(window_);
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display ? display : SDL_GetPrimaryDisplay());
    std::string screen = mode ? std::to_string(mode->w) + "x" + std::to_string(mode->h) : "?";
    std::string shown = wp::options::shown_value("video.scale", std::to_string(scale));
    std::string label = format(t.res_label, shown);
    std::string detail = format(t.res_detail, screen, wp::options::shown_value("video.scale", std::to_string(current)));
    if (slowed) {
        detail += " " + std::string(t.res_slow);
    }
    row(width, label.c_str(), detail.c_str(), 110.0f, 36.0f, [&] {
        if (ui::button(scale == current ? t.res_in_use : t.res_use, Kind::Secondary, 110.0f, scale != current)) {
            store("video.scale", std::to_string(scale));
        }
    });
    inset_separator(width);
    bool portable = !portable_folder().empty();
    row(width, t.portable_label, t.portable_detail, 42.0f, 24.0f, [&] {
        if (ui::toggle("portable", portable)) {
            toggle_portable(!portable);
        }
    });
    if (portable && project_.root != portable_folder() / "game") {
        inset_separator(width);
        std::string move = format(t.portable_move_detail, utf8_of(project_.root));
        std::string moving = format(t.portable_moving, std::to_string(task_percent_.load()));
        row(width, t.portable_move_label, move.c_str(), 150.0f, 36.0f, [&] {
            if (ui::button(state == 1 ? moving.c_str() : t.portable_move, Kind::Secondary, 150.0f, state == 0 && !runner_.running())) {
                move_next_to_launcher();
            }
        });
    }
    inset_separator(width);
    row(width, t.uninstall_label, t.uninstall_detail, 150.0f, 36.0f, [&] {
        if (ui::button(state == 2 ? t.uninstalling : t.uninstall_button, Kind::Secondary, 150.0f, state == 0 && !runner_.running() && project_.built())) {
            uninstall_confirm_ = true;
            uninstall_keep_ = true;
        }
    });
    tools.end();
    if (!system_message_.empty()) {
        ui::gap(14);
        ui::text(system_message_.c_str(), Font::Regular, ui::size::kDetail, p.secondary, width);
    }
    if (state == 1 || state == 2) {
        ui::request_frames(2);
    }
    uninstall_modal();
}
