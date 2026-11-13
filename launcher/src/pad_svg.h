#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct NSVGimage;
struct NSVGshape;

// Draws a controller SVG (prepared by tools/pad_svg.py) and lights up the parts that are pressed.
// Parts are the shapes whose id starts with '@': "@south", "@dpad_up", "@stick_l" ... The SVG is
// rasterized once per size and colour into small textures, so a frame only blits a few images.
class PadSvg {
public:
    struct Pose {
        std::unordered_map<std::string, float> lit;  // part name -> 0..1
        ImVec2 stick_left{0, 0};                      // -1..1
        ImVec2 stick_right{0, 0};
    };

    PadSvg() = default;
    PadSvg(const PadSvg&) = delete;
    PadSvg& operator=(const PadSvg&) = delete;
    ~PadSvg();

    // Rasterizes the SVG so it is `width` logical pixels wide. Does nothing if it is already built
    // for these arguments.
    bool build(SDL_Renderer* renderer, std::string_view svg, float width, ImU32 accent);
    bool ready() const { return base_ != nullptr; }
    ImVec2 size() const { return size_; }
    void draw(ImDrawList* list, ImVec2 at, const Pose& pose, float stick_travel) const;
    void release();

private:
    struct Part {
        std::string name;
        std::vector<NSVGshape*> shapes;
        std::vector<NSVGshape*> colour;
        std::vector<unsigned> original;
        ImVec2 min{0, 0};  // texture-space box in logical pixels
        ImVec2 max{0, 0};
        SDL_Texture* normal = nullptr;
        SDL_Texture* lit = nullptr;
        bool moving = false;
    };

    NSVGimage* image_ = nullptr;
    std::vector<Part> parts_;
    SDL_Texture* base_ = nullptr;
    ImVec2 size_{0, 0};
    float scale_ = 0;  // raster pixels per SVG unit
    float width_ = 0;
    ImU32 accent_ = 0;
    std::string key_;
};
