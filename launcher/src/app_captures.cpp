#include "app.h"

#include <algorithm>

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
        return;
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
        if (ImGui::InvisibleButton("capture", ImVec2(cell, height))) {
            SDL_OpenURL(file_url(capture.path).c_str());
        }
        bool hovered = ImGui::IsItemHovered();
        ImDrawList* list = ImGui::GetWindowDrawList();
        list->AddRectFilled(at, at + ImVec2(cell, height), p.surface, px(10));
        if (capture.texture) {
            list->AddImageRounded(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(capture.texture))), at, at + ImVec2(cell, height),
                                  ImVec2(0, 0), ImVec2(1, 1), hovered ? IM_COL32(255, 255, 255, 230) : IM_COL32_WHITE, px(10));
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
}
