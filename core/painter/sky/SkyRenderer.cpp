#include "SkyRenderer.h"

namespace mgd {

void SkyRenderer::render(Framebuffer& fb) const {
    int h = fb.height();
    int w = fb.width();
    if (h <= 0) return;

    for (int y = 0; y < h; ++y) {
        float t = static_cast<float>(y) / (h - 1);
        uint8_t r = static_cast<uint8_t>(top_color.r + (bottom_color.r - top_color.r) * t);
        uint8_t g = static_cast<uint8_t>(top_color.g + (bottom_color.g - top_color.g) * t);
        uint8_t b = static_cast<uint8_t>(top_color.b + (bottom_color.b - top_color.b) * t);
        RGBA lineColor(r, g, b, 255);

        uint8_t* row = fb.data() + static_cast<size_t>(y) * fb.stride();
        for (int x = 0; x < w; ++x) {
            row[x * 4 + 0] = lineColor.r;
            row[x * 4 + 1] = lineColor.g;
            row[x * 4 + 2] = lineColor.b;
            row[x * 4 + 3] = lineColor.a;
        }
    }
}

void SkyRenderer::setColors(const RGBA& top, const RGBA& bottom) {
    top_color = top;
    bottom_color = bottom;
}

} // namespace mgd
