#pragma once

#include <algorithm>
#include <cmath>

#include "ui.h"

using ui::Font;
using ui::Kind;
using ui::px;

class Card {
public:
    explicit Card(float width) : list_(ImGui::GetWindowDrawList()), start_(ImGui::GetCursorScreenPos()), width_(width) {
        list_->ChannelsSplit(2);
        list_->ChannelsSetCurrent(1);
    }

    void end() {
        float bottom = ImGui::GetCursorScreenPos().y;
        list_->ChannelsSetCurrent(0);
        list_->AddRectFilled(start_, ImVec2(start_.x + width_, bottom), ui::palette().surface, px(14));
        list_->ChannelsMerge();
    }

private:
    ImDrawList* list_;
    ImVec2 start_;
    float width_;
};

template <typename Control>
inline void row(float width, const char* label, const char* detail, float control_width, float control_height, Control control) {
    const ui::Palette& p = ui::palette();
    ImVec2 top = ImGui::GetCursorScreenPos();
    float pad = px(18);
    float text_width = width - pad * 2 - px(control_width) - px(24);
    float content = ui::text_size(label, Font::Regular, ui::size::kBody, text_width).y;
    if (detail && *detail) {
        content += px(3) + ui::text_size(detail, Font::Regular, ui::size::kDetail, text_width).y;
    }
    float height = std::max(content + px(28), px(control_height) + px(26));
    ImGui::SetCursorScreenPos(top + ImVec2(pad, std::round((height - content) * 0.5f)));
    ImGui::BeginGroup();
    ui::text(label, Font::Regular, ui::size::kBody, p.text, text_width);
    if (detail && *detail) {
        ui::gap(3);
        ui::text(detail, Font::Regular, ui::size::kDetail, p.secondary, text_width);
    }
    ImGui::EndGroup();
    float bottom = top.y + height;
    ImGui::SetCursorScreenPos(ImVec2(top.x + width - pad - px(control_width), top.y + std::round((height - px(control_height)) * 0.5f)));
    control();
    ImGui::SetCursorScreenPos(ImVec2(top.x, bottom));
    ImGui::Dummy(ImVec2(width, 0));
}

inline void inset_separator(float width) {
    ImVec2 at = ImGui::GetCursorScreenPos();
    float thickness = std::max(1.0f, std::floor(ui::scale()));
    ImGui::GetWindowDrawList()->AddRectFilled(at + ImVec2(px(18), 0), at + ImVec2(width - px(18), thickness), ui::palette().separator);
    ImGui::Dummy(ImVec2(width, thickness));
}
