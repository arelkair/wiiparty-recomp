#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <filesystem>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include "project.h"
#include "runner.h"
#include "toolchain.h"
#include "ui.h"
#include "updater.h"
#include "wp/save_backup.h"

struct TexturePack {
    std::string name;
    bool enabled = false;
    size_t files = 0;
    uintmax_t bytes = 0;
};

struct DolphinFind {
    std::filesystem::path wii;
    bool has_save = false;
    int miis = 0;
    bool scanned = false;
};

struct Capture {
    std::filesystem::path path;
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
    bool tried = false;
    bool selected = false;
};

enum class Page { Game, Settings, Saves, Controls, Textures, Captures, System, Licenses, Count };

class App {
public:
    App(SDL_Window* window, Project project, bool install_mode);
    ~App();

    bool consume(const SDL_Event& event);
    void frame();
    void set_page(Page page);
    bool busy() const;
    bool quit() const;
    int exit_code() const;
    void apply_theme();
    void add_on_start(const std::string& disc);
    bool use_version_now(const std::string& id);

private:
    void use_root(const std::filesystem::path& root);
    void store(const std::string& key, const std::string& value);
    std::string value(const std::string& key) const;
    void wake();

    void sidebar(float height);
    void page(float width, float height);
    void game_page(float width, float height);
    void play_panel(float width, float height);
    void install_panel(float width);
    void progress_panel(float width, float height);
    void settings_page(float width);
    void saves_page(float width);
    void controls_page(float width);
    void licenses_page(float width, float height);
    void textures_page(float width);
    void versions_section(float width);
    void gamepads_section(float width);
    void wiimotes_section(float width);
    void tester_section(float width);
    void dolphin_section(float width);
    void captures_page(float width);
    void list_captures();
    void load_capture(Capture& capture);
    void release_captures();
    void capture_viewer();
    void capture_delete_modal();
    void open_capture_view(const std::vector<std::filesystem::path>& targets, size_t index);
    void load_view_texture();
    void close_capture_view();
    bool crop_target(const std::filesystem::path& path);
    void system_page(float width);
    std::vector<std::pair<std::string, std::string>> diagnostics();
    int recommended_scale(bool& slowed);
    void toggle_portable(bool on);
    void move_next_to_launcher();
    void uninstall(bool keep_data);
    void uninstall_modal();
    void scan_dolphin();
    void import_dolphin();
    void pad_map_section(float width);
    void track_held(const SDL_Event& event);
    uint32_t keyboard_buttons();
    uint32_t gamepad_buttons(SDL_Gamepad* pad);
    void whats_new_card(float width);
    void create_report();
    void choose_added_disc();
    void switch_version(const std::string& id);
    void restore_modal();
    void launcher_update_card(float width);
    void repair_modal();
    void repair();
    void start_update_check();
    void start_estimate();
    void update_launcher();
    void heading(const char* title, const char* intro, float width);

    void choose_disc();
    void choose_folder();
    void install(bool adding = false);
    void finish_install();
    void play();
    void refresh_backups();
    void bind(size_t action, const std::string& name);
    void stop_capture();

    SDL_Window* window_;
    Project project_;
    Toolchain toolchain_;
    Runner runner_;
    bool install_mode_;
    bool quit_ = false;
    int exit_code_ = 0;
    Page page_ = Page::Game;
    bool progress_view_ = false;
    bool was_running_ = false;
    bool show_details_ = false;
    bool started_ = false;
    std::vector<std::string> log_;
    std::string result_;
    Outcome outcome_ = Outcome::None;
    std::string disc_;
    std::vector<wp::saves::Backup> backups_;
    std::vector<std::string> backup_sizes_;
    std::string saves_message_;
    int restore_index_ = -1;
    int license_ = 0;
    int capturing_ = -1;
    bool capture_reserved_ = false;
    bool swallow_release_ = false;
    ImRect capture_rect_;
    std::mutex picked_mutex_;
    std::string picked_disc_;
    std::string picked_folder_;
    std::string picked_added_disc_;
    std::string add_disc_;
    std::vector<TexturePack> packs_;
    bool packs_dirty_ = true;
    std::string versions_message_;
    bool versions_failed_ = false;
    int last_step_ = -1;
    std::thread download_thread_;
    std::string disc_line_;
    bool repair_confirm_ = false;
    std::string launcher_message_;
    bool launcher_message_failed_ = false;
    std::atomic<int> launcher_state_{0};
    std::string launcher_error_;
    bool swap_pending_ = false;
    std::thread pair_thread_;
    std::atomic<int> pair_state_{0};
    std::atomic<int> pair_count_{0};
    int access_result_ = 0;
    std::set<int> held_codes_;
    DolphinFind dolphin_;
    std::vector<Capture> captures_;
    size_t captures_total_ = 0;
    bool captures_listed_ = false;
    bool capture_view_open_ = false;
    bool captures_selected_mode_ = false;
    std::vector<std::filesystem::path> capture_targets_;
    size_t capture_target_ = 0;
    SDL_Texture* view_texture_ = nullptr;
    int view_width_ = 0;
    int view_height_ = 0;
    ImVec2 crop_from_{0.0f, 0.0f};
    ImVec2 crop_to_{0.0f, 0.0f};
    bool crop_dragging_ = false;
    bool captures_delete_open_ = false;
    std::vector<std::filesystem::path> captures_delete_paths_;
    std::vector<std::pair<std::string, std::string>> system_lines_;
    int system_scale_ = 1;
    bool system_slowed_ = false;
    bool system_ready_ = false;
    bool diagnostics_copied_ = false;
    std::string system_message_;
    std::thread task_thread_;
    std::mutex task_mutex_;
    std::atomic<int> task_state_{0};
    std::atomic<int> task_percent_{0};
    std::string task_error_;
    bool uninstall_confirm_ = false;
    bool uninstall_keep_ = true;
    std::string picked_dolphin_;
    std::map<SDL_JoystickID, SDL_Gamepad*> tester_pads_;
};

void wake_main_loop();
SDL_JoystickID* gamepads(int& count);
