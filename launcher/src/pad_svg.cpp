#include "pad_svg.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"

namespace {

constexpr float kSuper = 2.0f;   // raster pixels per logical pixel
constexpr float kMargin = 4.0f;  // SVG units around a part for its stroke

// "@south" -> part "south" (a colour shape); "@south_12" -> part "south" (any other shape).
bool part_of(const char* id, std::string& name, bool& colour) {
    if (id[0] != '@') {
        return false;
    }
    name = id + 1;
    colour = true;
    size_t cut = name.rfind('_');
    if (cut != std::string::npos && cut + 1 < name.size() && std::all_of(name.begin() + static_cast<long>(cut) + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        name.resize(cut);
        colour = false;
    }
    return true;
}

ImTextureRef reference(SDL_Texture* texture) {
    return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture)));
}

void hide_all(NSVGimage* image) {
    for (NSVGshape* shape = image->shapes; shape; shape = shape->next) {
        shape->flags &= static_cast<unsigned char>(~NSVG_FLAGS_VISIBLE);
    }
}

void show(const std::vector<NSVGshape*>& shapes) {
    for (NSVGshape* shape : shapes) {
        shape->flags |= NSVG_FLAGS_VISIBLE;
    }
}

}  // namespace

PadSvg::~PadSvg() {
    release();
}

void PadSvg::release() {
    for (Part& part : parts_) {
        if (part.normal) {
            SDL_DestroyTexture(part.normal);
        }
        if (part.lit) {
            SDL_DestroyTexture(part.lit);
        }
    }
    parts_.clear();
    if (base_) {
        SDL_DestroyTexture(base_);
        base_ = nullptr;
    }
    if (image_) {
        nsvgDelete(image_);
        image_ = nullptr;
    }
    key_.clear();
}

// Rasterizes the shapes that are currently visible into a texture covering [min, max] (logical pixels).
static SDL_Texture* rasterize(SDL_Renderer* renderer, NSVGimage* image, float scale, ImVec2 min, ImVec2 max) {
    int width = std::max(1, static_cast<int>(std::ceil((max.x - min.x) * kSuper)));
    int height = std::max(1, static_cast<int>(std::ceil((max.y - min.y) * kSuper)));
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (!rasterizer) {
        return nullptr;
    }
    nsvgRasterize(rasterizer, image, -min.x * kSuper, -min.y * kSuper, scale, pixels.data(), width, height, width * 4);
    nsvgDeleteRasterizer(rasterizer);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture) {
        return nullptr;
    }
    SDL_UpdateTexture(texture, nullptr, pixels.data(), width * 4);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}

bool PadSvg::build(SDL_Renderer* renderer, std::string_view svg, float width, ImU32 accent) {
    std::string key = std::to_string(static_cast<long>(std::lround(width))) + "/" + std::to_string(accent) + "/" + std::to_string(reinterpret_cast<uintptr_t>(svg.data()));
    if (key == key_ && base_) {
        return true;
    }
    release();
    if (!renderer || svg.empty() || width < 16) {
        return false;
    }
    std::string text(svg);
    image_ = nsvgParse(text.data(), "px", 96.0f);
    if (!image_ || image_->width <= 0 || image_->height <= 0) {
        release();
        return false;
    }
    width_ = width;
    scale_ = width * kSuper / image_->width;
    size_ = ImVec2(width, image_->height * scale_ / kSuper);
    accent_ = accent;
    auto to_logical = [&](float units) { return units * scale_ / kSuper; };

    for (NSVGshape* shape = image_->shapes; shape; shape = shape->next) {
        std::string name;
        bool colour = false;
        if (!part_of(shape->id, name, colour)) {
            continue;
        }
        auto found = std::find_if(parts_.begin(), parts_.end(), [&](const Part& part) { return part.name == name; });
        if (found == parts_.end()) {
            parts_.push_back(Part{});
            found = parts_.end() - 1;
            found->name = name;
            found->min = ImVec2(1e9f, 1e9f);
            found->max = ImVec2(-1e9f, -1e9f);
            found->moving = name.rfind("stick_", 0) == 0;
        }
        found->shapes.push_back(shape);
        if (colour) {
            found->colour.push_back(shape);
        }
        found->min.x = std::min(found->min.x, to_logical(shape->bounds[0] - kMargin));
        found->min.y = std::min(found->min.y, to_logical(shape->bounds[1] - kMargin));
        found->max.x = std::max(found->max.x, to_logical(shape->bounds[2] + kMargin));
        found->max.y = std::max(found->max.y, to_logical(shape->bounds[3] + kMargin));
    }

    // Base: everything except the parts that move.
    for (const Part& part : parts_) {
        if (part.moving) {
            for (NSVGshape* shape : part.shapes) {
                shape->flags &= static_cast<unsigned char>(~NSVG_FLAGS_VISIBLE);
            }
        }
    }
    base_ = rasterize(renderer, image_, scale_, ImVec2(0, 0), size_);
    for (NSVGshape* shape = image_->shapes; shape; shape = shape->next) {
        shape->flags |= NSVG_FLAGS_VISIBLE;
    }

    for (Part& part : parts_) {
        hide_all(image_);
        show(part.shapes);
        if (part.moving) {
            part.normal = rasterize(renderer, image_, scale_, part.min, part.max);
        }
        std::vector<NSVGpaint> saved;
        for (NSVGshape* shape : part.colour) {
            saved.push_back(shape->fill);
            shape->fill.type = NSVG_PAINT_COLOR;
            shape->fill.color = accent;
        }
        part.lit = rasterize(renderer, image_, scale_, part.min, part.max);
        for (size_t i = 0; i < part.colour.size(); i++) {
            part.colour[i]->fill = saved[i];
        }
    }
    for (NSVGshape* shape = image_->shapes; shape; shape = shape->next) {
        shape->flags |= NSVG_FLAGS_VISIBLE;
    }
    key_ = key;
    return base_ != nullptr;
}

void PadSvg::draw(ImDrawList* list, ImVec2 at, const Pose& pose, float stick_travel) const {
    if (!base_) {
        return;
    }
    list->AddImage(reference(base_), at, at + size_);
    auto value = [&](const std::string& name) {
        auto found = pose.lit.find(name);
        return found == pose.lit.end() ? 0.0f : std::clamp(found->second, 0.0f, 1.0f);
    };
    auto has = [&](const char* name) {
        return std::any_of(parts_.begin(), parts_.end(), [&](const Part& part) { return part.name == name; });
    };
    const bool split_dpad = has("dpad_up");
    for (const Part& part : parts_) {
        if (!part.lit) {
            continue;
        }
        ImVec2 a = at + part.min;
        ImVec2 b = at + part.max;
        if (part.moving) {
            ImVec2 move = part.name == "stick_l" ? pose.stick_left : pose.stick_right;
            ImVec2 offset(move.x * stick_travel, move.y * stick_travel);
            SDL_Texture* texture = value(part.name) > 0.5f ? part.lit : part.normal;
            if (texture) {
                list->AddImage(reference(texture), a + offset, b + offset);
            }
            continue;
        }
        if (part.name == "dpad") {
            if (split_dpad) {
                continue;
            }
            // One shape for the whole cross: light only the arm that is pressed.
            const char* arms[4] = {"up", "down", "left", "right"};
            for (int i = 0; i < 4; i++) {
                float lit = value(std::string("dpad_") + arms[i]);
                if (lit <= 0.01f) {
                    continue;
                }
                float third_x = (b.x - a.x) / 3.0f;
                float third_y = (b.y - a.y) / 3.0f;
                ImVec2 clip_a = a;
                ImVec2 clip_b = b;
                if (i == 0) {
                    clip_b.y = a.y + third_y;
                } else if (i == 1) {
                    clip_a.y = b.y - third_y;
                } else if (i == 2) {
                    clip_b.x = a.x + third_x;
                } else {
                    clip_a.x = b.x - third_x;
                }
                list->PushClipRect(clip_a, clip_b, true);
                list->AddImage(reference(part.lit), a, b);
                list->PopClipRect();
            }
            continue;
        }
        float lit = value(part.name);
        if (lit > 0.01f) {
            list->AddImage(reference(part.lit), a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, static_cast<int>(lit * 255.0f)));
        }
    }
}
