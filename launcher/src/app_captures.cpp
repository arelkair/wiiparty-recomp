#include "app.h"

#include <algorithm>
#include <cmath>

#include "layout.h"
#include "subprocess.h"
#include "texts.h"
#include "wp/screenshot.h"

namespace {

namespace fs = std::filesystem;

constexpr size_t kShownCaptures = 60;
constexpr int kLoadsPerFrame = 2;
constexpr int kThumbnailWidth = 360;
constexpr int kColumns = 3;

std::string file_url(const fs::path& path) {
    std::string url = "file:///" + utf8_of(path.lexically_normal());
    std::replace(url.begin(), url.end(), '\\', '/');
    return url;
}

}

void App::release_captures() {
    close_capture_view();
    for (Capture& capture : captures_) {
        if (capture.texture) {
            SDL_DestroyTexture(capture.texture);
        }
    }
    captures_.clear();
    captures_total_ = 0;
    captures_listed_ = false;
}

void App::list_captures() {
    release_captures();
    std::error_code error;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(project_.root / wp::screenshot::kFolder, error)) {
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (entry.is_regular_file(error) && extension == ".png") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) { return a.filename() > b.filename(); });
    captures_total_ = files.size();
    for (size_t i = 0; i < files.size() && i < kShownCaptures; i++) {
        Capture capture;
        capture.path = files[i];
        captures_.push_back(capture);
    }
    captures_listed_ = true;
}

void App::load_capture(Capture& capture) {
    capture.tried = true;
    SDL_Surface* image = SDL_LoadPNG(utf8_of(capture.path).c_str());
    if (!image) {
        return;
    }
    int width = std::min(image->w, kThumbnailWidth);
    int height = std::max(1, image->h * width / std::max(1, image->w));
    SDL_Surface* small = SDL_ScaleSurface(image, width, height, SDL_SCALEMODE_LINEAR);
    SDL_Renderer* renderer = SDL_GetRenderer(window_);
    if (small && renderer) {
        capture.texture = SDL_CreateTextureFromSurface(renderer, small);
        capture.width = image->w;
        capture.height = image->h;
    }
    SDL_DestroySurface(small);
    SDL_DestroySurface(image);
}

void App::load_view_texture() {
    if (view_texture_) {
        SDL_DestroyTexture(view_texture_);
        view_texture_ = nullptr;
    }
    view_width_ = 0;
    view_height_ = 0;
    crop_from_ = ImVec2(0.0f, 0.0f);
    crop_to_ = ImVec2(0.0f, 0.0f);
    if (capture_target_ >= capture_targets_.size()) {
        return;
    }
    SDL_Surface* image = SDL_LoadPNG(utf8_of(capture_targets_[capture_target_]).c_str());
    SDL_Renderer* renderer = SDL_GetRenderer(window_);
    if (image && renderer) {
        view_texture_ = SDL_CreateTextureFromSurface(renderer, image);
        view_width_ = image->w;
        view_height_ = image->h;
    }
    SDL_DestroySurface(image);
}

void App::open_capture_view(const std::vector<fs::path>& targets, size_t index) {
    capture_targets_ = targets;
    capture_target_ = index;
    capture_view_open_ = !targets.empty();
    crop_dragging_ = false;
    load_view_texture();
}

void App::close_capture_view() {
    capture_view_open_ = false;
    capture_targets_.clear();
    if (view_texture_) {
        SDL_DestroyTexture(view_texture_);
        view_texture_ = nullptr;
    }
}

bool App::crop_target(const fs::path& path) {
    SDL_Surface* image = SDL_LoadPNG(utf8_of(path).c_str());
    if (!image) {
        return false;
    }
    float left = std::min(crop_from_.x, crop_to_.x);
    float right = std::max(crop_from_.x, crop_to_.x);
    float top = std::min(crop_from_.y, crop_to_.y);
    float bottom = std::max(crop_from_.y, crop_to_.y);
    SDL_Rect area;
    area.x = static_cast<int>(std::floor(left * static_cast<float>(image->w)));
    area.y = static_cast<int>(std::floor(top * static_cast<float>(image->h)));
    area.w = std::max(1, static_cast<int>(std::round((right - left) * static_cast<float>(image->w))));
    area.h = std::max(1, static_cast<int>(std::round((bottom - top) * static_cast<float>(image->h))));
    area.w = std::min(area.w, image->w - area.x);
    area.h = std::min(area.h, image->h - area.y);
    bool saved = false;
    if (area.w > 0 && area.h > 0) {
        SDL_Surface* cut = SDL_CreateSurface(area.w, area.h, image->format);
        if (cut && SDL_BlitSurface(image, &area, cut, nullptr)) {
            fs::path target;
            std::error_code error;
            for (int number = 1; number < 1000; number++) {
                std::string suffix = number == 1 ? "-crop" : "-crop-" + std::to_string(number);
                target = path.parent_path() / (path.stem().string() + suffix + path.extension().string());
                if (!fs::exists(target, error)) {
                    break;
                }
            }
            saved = SDL_SavePNG(cut, utf8_of(target).c_str());
        }
        SDL_DestroySurface(cut);
    }
    SDL_DestroySurface(image);
    return saved;
}

void App::capture_viewer() {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    if (!capture_view_open_) {
        return;
    }
    if (!ImGui::IsPopupOpen("capture_view")) {
        ImGui::OpenPopup("capture_view");
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float modal_width = std::min(viewport->Size.x - px(48), px(980));
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(modal_width, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(20), px(18)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, px(16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(16));
    bool close = false;
    if (ImGui::BeginPopupModal("capture_view", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        float inner = modal_width - px(40);
        bool single = !captures_selected_mode_;
        std::string name = capture_target_ < capture_targets_.size() ? utf8_of(capture_targets_[capture_target_].filename()) : std::string();
        std::string title = name + "  (" + std::to_string(capture_target_ + 1) + "/" + std::to_string(capture_targets_.size()) + ")";
        ui::text(title.c_str(), Font::Semibold, ui::size::kBody, p.text, inner);
        ui::gap(10);
        float max_height = std::max(px(160), viewport->Size.y - px(270));
        float aspect = view_width_ > 0 ? static_cast<float>(view_width_) / static_cast<float>(view_height_) : 1.6f;
        float draw_width = std::min(inner, max_height * aspect);
        float draw_height = draw_width / aspect;
        ImVec2 start = ImGui::GetCursorScreenPos();
        ImVec2 at(start.x + std::floor((inner - draw_width) * 0.5f), start.y);
        ImGui::SetCursorScreenPos(at);
        ImGui::InvisibleButton("area", ImVec2(draw_width, draw_height));
        ImDrawList* list = ImGui::GetWindowDrawList();
        list->AddRectFilled(at, at + ImVec2(draw_width, draw_height), p.surface, px(8));
        if (view_texture_) {
            list->AddImageRounded(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(view_texture_))), at, at + ImVec2(draw_width, draw_height),
                                  ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, px(8));
        }
        ImVec2 mouse = ImGui::GetIO().MousePos;
        ImVec2 relative(std::clamp((mouse.x - at.x) / draw_width, 0.0f, 1.0f), std::clamp((mouse.y - at.y) / draw_height, 0.0f, 1.0f));
        if (ImGui::IsItemActivated()) {
            crop_from_ = relative;
            crop_to_ = relative;
            crop_dragging_ = true;
        }
        if (crop_dragging_) {
            if (ImGui::IsMouseDown(0)) {
                crop_to_ = relative;
            } else {
                crop_dragging_ = false;
            }
        }
        float area_width = std::abs(crop_to_.x - crop_from_.x);
        float area_height = std::abs(crop_to_.y - crop_from_.y);
        bool has_area = area_width > 0.02f && area_height > 0.02f;
        if (has_area) {
            ImVec2 a = at + ImVec2(std::min(crop_from_.x, crop_to_.x) * draw_width, std::min(crop_from_.y, crop_to_.y) * draw_height);
            ImVec2 b = at + ImVec2(std::max(crop_from_.x, crop_to_.x) * draw_width, std::max(crop_from_.y, crop_to_.y) * draw_height);
            ImU32 dim = IM_COL32(0, 0, 0, 140);
            list->AddRectFilled(at, ImVec2(at.x + draw_width, a.y), dim);
            list->AddRectFilled(ImVec2(at.x, b.y), at + ImVec2(draw_width, draw_height), dim);
            list->AddRectFilled(ImVec2(at.x, a.y), ImVec2(a.x, b.y), dim);
            list->AddRectFilled(ImVec2(b.x, a.y), ImVec2(at.x + draw_width, b.y), dim);
            list->AddRect(a, b, p.accent, 0.0f, px(2));
        }
        ImGui::SetCursorScreenPos(ImVec2(start.x, at.y + draw_height));
        ui::gap(12);
        ui::text(t.captures_crop_hint, Font::Regular, ui::size::kDetail, p.secondary, inner);
        ui::gap(14);
        ImVec2 row = ImGui::GetCursorScreenPos();
        if (single && capture_targets_.size() > 1) {
            if (ui::button("<", Kind::Secondary, 44.0f) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
                capture_target_ = (capture_target_ + capture_targets_.size() - 1) % capture_targets_.size();
                load_view_texture();
            }
            ImGui::SetCursorScreenPos(ImVec2(row.x + px(52), row.y));
            if (ui::button(">", Kind::Secondary, 44.0f) || ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
                capture_target_ = (capture_target_ + 1) % capture_targets_.size();
                load_view_texture();
            }
        }
        std::string save_label = format(t.captures_save_crops, std::to_string(single ? 1 : capture_targets_.size()));
        float save_width = 240.0f;
        float close_width = 110.0f;
        float clear_width = 140.0f;
        float right = row.x + inner;
        ImGui::SetCursorScreenPos(ImVec2(right - px(close_width), row.y));
        if (ui::button(t.captures_close, Kind::Ghost, close_width) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            close = true;
        }
        ImGui::SetCursorScreenPos(ImVec2(right - px(close_width) - px(8) - px(save_width), row.y));
        if (ui::button(save_label.c_str(), Kind::Primary, save_width, has_area)) {
            if (single) {
                if (capture_target_ < capture_targets_.size()) {
                    crop_target(capture_targets_[capture_target_]);
                }
            } else {
                for (const fs::path& path : capture_targets_) {
                    crop_target(path);
                }
            }
            close = true;
            captures_listed_ = false;
        }
        ImGui::SetCursorScreenPos(ImVec2(right - px(close_width) - px(8) - px(save_width) - px(8) - px(clear_width), row.y));
        if (ui::button(t.captures_clear_area, Kind::Secondary, clear_width, has_area)) {
            crop_from_ = ImVec2(0.0f, 0.0f);
            crop_to_ = ImVec2(0.0f, 0.0f);
        }
        ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + px(36)));
        ImGui::Dummy(ImVec2(inner, 0));
        if (close) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
    if (close) {
        close_capture_view();
    }
}

void App::capture_delete_modal() {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    if (captures_delete_open_ && !ImGui::IsPopupOpen("capture_delete")) {
        ImGui::OpenPopup("capture_delete");
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(px(440), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(24), px(22)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, px(16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, px(16));
    if (ImGui::BeginPopupModal("capture_delete", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        float inner = px(440) - px(48);
        ui::text(t.captures_delete_title, Font::Semibold, 17.0f, p.text);
        ui::gap(8);
        ui::text(format(t.captures_delete_question, std::to_string(captures_delete_paths_.size())).c_str(), Font::Regular, ui::size::kBody, p.secondary, inner);
        ui::gap(22);
        ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(at.x + inner - px(130) - px(10) - px(110), at.y));
        if (ui::button(t.cancel, Kind::Ghost, 110.0f)) {
            captures_delete_open_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetCursorScreenPos(ImVec2(at.x + inner - px(130), at.y));
        std::string label = format(t.captures_delete, std::to_string(captures_delete_paths_.size()));
        if (ui::button(label.c_str(), Kind::Primary, 130.0f)) {
            std::error_code error;
            for (const fs::path& path : captures_delete_paths_) {
                fs::remove(path, error);
            }
            captures_delete_paths_.clear();
            captures_delete_open_ = false;
            captures_listed_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + px(36)));
        ImGui::Dummy(ImVec2(inner, 0));
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
}

void App::captures_page(float width) {
    const ui::Palette& p = ui::palette();
    const Texts& t = texts();
    ImVec2 top = ImGui::GetCursorScreenPos();
    heading(t.page_captures, t.captures_intro, width - px(150));
    ImVec2 after = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(top.x + width - px(140), top.y));
    fs::path folder = project_.root / wp::screenshot::kFolder;
    if (ui::button(t.open_folder, Kind::Secondary, 140.0f)) {
        std::error_code error;
        fs::create_directories(folder, error);
        SDL_OpenURL(file_url(folder).c_str());
    }
    ImGui::SetCursorScreenPos(after);
    if (!captures_listed_) {
        list_captures();
    }
    if (captures_.empty()) {
        Card card(width);
        ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(at + ImVec2(px(18), px(18)));
        ui::text(t.captures_empty, Font::Regular, ui::size::kBody, p.secondary, width - px(36));
        ui::gap(18);
        card.end();
        capture_delete_modal();
        return;
    }
    size_t selected = 0;
    for (const Capture& capture : captures_) {
        selected += capture.selected ? 1 : 0;
    }
    {
        ImVec2 row = ImGui::GetCursorScreenPos();
        bool all = selected == captures_.size();
        float x = row.x;
        if (ui::button(all ? t.captures_select_none : t.captures_select_all, Kind::Secondary, 180.0f)) {
            for (Capture& capture : captures_) {
                capture.selected = !all;
            }
        }
        x += px(188);
        ImGui::SetCursorScreenPos(ImVec2(x, row.y));
        std::string crop_label = format(t.captures_crop, std::to_string(selected));
        if (ui::button(crop_label.c_str(), Kind::Secondary, 150.0f, selected > 0)) {
            std::vector<fs::path> targets;
            for (const Capture& capture : captures_) {
                if (capture.selected) {
                    targets.push_back(capture.path);
                }
            }
            captures_selected_mode_ = true;
            open_capture_view(targets, 0);
        }
        x += px(158);
        ImGui::SetCursorScreenPos(ImVec2(x, row.y));
        std::string delete_label = format(t.captures_delete, std::to_string(selected));
        if (ui::button(delete_label.c_str(), Kind::Secondary, 150.0f, selected > 0)) {
            captures_delete_paths_.clear();
            for (const Capture& capture : captures_) {
                if (capture.selected) {
                    captures_delete_paths_.push_back(capture.path);
                }
            }
            captures_delete_open_ = true;
        }
        ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + px(36) + px(8)));
        ui::text(t.captures_view_hint, Font::Regular, ui::size::kDetail, p.secondary, width);
        ui::gap(12);
    }
    int loads = 0;
    float gap = px(12);
    float cell = std::floor((width - gap * (kColumns - 1)) / kColumns);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float row_top = origin.y;
    float row_height = 0.0f;
    for (size_t i = 0; i < captures_.size(); i++) {
        Capture& capture = captures_[i];
        if (!capture.tried && loads < kLoadsPerFrame) {
            load_capture(capture);
            loads++;
        }
        int column = static_cast<int>(i % kColumns);
        if (column == 0 && i > 0) {
            row_top += row_height + gap;
            row_height = 0.0f;
        }
        float height = capture.width > 0 ? cell * static_cast<float>(capture.height) / static_cast<float>(capture.width) : cell * 0.75f;
        row_height = std::max(row_height, height);
        ImVec2 at(origin.x + column * (cell + gap), row_top);
        ImGui::SetCursorScreenPos(at);
        ImGui::PushID(static_cast<int>(i));
        bool clicked = ImGui::InvisibleButton("capture", ImVec2(cell, height));
        bool hovered = ImGui::IsItemHovered();
        if (hovered && ImGui::IsMouseDoubleClicked(0)) {
            capture.selected = !capture.selected;
            std::vector<fs::path> all;
            for (const Capture& other : captures_) {
                all.push_back(other.path);
            }
            captures_selected_mode_ = false;
            open_capture_view(all, i);
        } else if (clicked) {
            capture.selected = !capture.selected;
        }
        ImDrawList* list = ImGui::GetWindowDrawList();
        list->AddRectFilled(at, at + ImVec2(cell, height), p.surface, px(10));
        if (capture.texture) {
            list->AddImageRounded(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(capture.texture))), at, at + ImVec2(cell, height),
                                  ImVec2(0, 0), ImVec2(1, 1), hovered ? IM_COL32(255, 255, 255, 230) : IM_COL32_WHITE, px(10));
        }
        if (capture.selected) {
            list->AddRect(at, at + ImVec2(cell, height), p.accent, px(10), px(3));
            ImVec2 badge = at + ImVec2(px(20), px(20));
            list->AddCircleFilled(badge, px(11), p.accent);
            ui::check_mark(badge, px(10), p.primary_text);
        }
        if (hovered) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, row_top + row_height + px(16)));
    if (captures_total_ > captures_.size()) {
        std::string more = format(t.captures_more, std::to_string(captures_.size()), std::to_string(captures_total_));
        ui::text(more.c_str(), Font::Regular, ui::size::kDetail, p.secondary, width);
    }
    ImGui::Dummy(ImVec2(width, 0));
    if (loads > 0) {
        ui::request_frames(2);
    }
    capture_viewer();
    capture_delete_modal();
}
