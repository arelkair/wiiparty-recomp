#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "project.h"
#include "runner.h"
#include "toolchain.h"
#include "ui.h"
#include "wp/save_backup.h"

enum class Page { Game, Settings, Saves, Controls, Licenses, Count };

class App {
public:
    App(SDL_Window* window, Project project, bool install_mode);

    bool consume(const SDL_Event& event);
    void frame();
    void set_page(Page page);
    bool busy() const;
    bool quit() const;
    int exit_code() const;
    void apply_theme();

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
    void restore_modal();
    void heading(const char* title, const char* intro, float width);

    void choose_disc();
    void choose_folder();
    void install();
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
    int last_step_ = -1;
};

void wake_main_loop();
