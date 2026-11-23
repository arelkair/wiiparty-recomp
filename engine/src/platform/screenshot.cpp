#include "wp/screenshot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace wp::screenshot {

std::string file_name(const char* prefix, const std::tm& time, int attempt) {
    char stamp[96];
    std::snprintf(stamp, sizeof stamp, "_%04d-%02d-%02d_%02d-%02d-%02d", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min,
                  time.tm_sec);
    std::string name = std::string(prefix) + stamp;
    if (attempt > 1) {
        name += "_" + std::to_string(attempt);
    }
    return name + ".png";
}

void fit_aspect(std::vector<uint32_t>& pixels, uint32_t& width, uint32_t& height, double aspect) {
    if (width == 0 || height == 0 || aspect <= 0.0 || pixels.size() != static_cast<size_t>(width) * height) {
        return;
    }
    uint32_t target = std::max<uint32_t>(1, static_cast<uint32_t>(std::lround(static_cast<double>(height) * aspect)));
    if (target == width) {
        return;
    }
    std::vector<uint32_t> fitted(static_cast<size_t>(target) * height);
    std::vector<uint32_t> left(target);
    std::vector<uint32_t> weight(target);
    for (uint32_t x = 0; x < target; x++) {
        double source = (x + 0.5) * width / target - 0.5;
        source = std::clamp(source, 0.0, static_cast<double>(width - 1));
        uint32_t index = static_cast<uint32_t>(source);
        left[x] = index;
        weight[x] = static_cast<uint32_t>((source - index) * 256.0);
    }
    for (uint32_t y = 0; y < height; y++) {
        const uint32_t* row = pixels.data() + static_cast<size_t>(y) * width;
        uint32_t* out = fitted.data() + static_cast<size_t>(y) * target;
        for (uint32_t x = 0; x < target; x++) {
            uint32_t a = row[left[x]];
            uint32_t b = row[std::min(left[x] + 1, width - 1)];
            uint32_t w = weight[x];
            uint32_t red = (((a >> 16) & 0xFF) * (256 - w) + ((b >> 16) & 0xFF) * w + 128) >> 8;
            uint32_t green = (((a >> 8) & 0xFF) * (256 - w) + ((b >> 8) & 0xFF) * w + 128) >> 8;
            uint32_t blue = ((a & 0xFF) * (256 - w) + (b & 0xFF) * w + 128) >> 8;
            out[x] = (red << 16) | (green << 8) | blue;
        }
    }
    pixels = std::move(fitted);
    width = target;
}

}
