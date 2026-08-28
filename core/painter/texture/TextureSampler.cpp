#include "TextureSampler.h"
#include <cmath>
#include <algorithm>

namespace mgd {

float TextureSampler::wrapCoordinate(float coord, int size, WrapMode mode) {
    if (mode == WrapMode::REPEAT) {
        coord = coord - std::floor(coord);
        if (coord < 0.0f) coord += 1.0f;
    } else {
        coord = std::clamp(coord, 0.0f, 1.0f);
    }
    return coord;
}

RGBA TextureSampler::sample(const TextureData& tex, float u, float v) {
    if (tex.width == 0 || tex.height == 0 || tex.pixels.empty()) {
        return {128, 128, 128, 255};
    }

    u = wrapCoordinate(u, tex.width, tex.sampler_state.wrap_u);
    v = wrapCoordinate(v, tex.height, tex.sampler_state.wrap_v);

    if (tex.sampler_state.filter == TextureFilter::NEAREST) {
        int px = static_cast<int>(u * tex.width) % tex.width;
        int py = static_cast<int>(v * tex.height) % tex.height;
        size_t idx = (static_cast<size_t>(py) * tex.width + px) * 4;
        return {tex.pixels[idx + 0], tex.pixels[idx + 1], tex.pixels[idx + 2], tex.pixels[idx + 3]};
    }

    float fx = u * tex.width - 0.5f;
    float fy = v * tex.height - 0.5f;

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    x0 = ((x0 % tex.width) + tex.width) % tex.width;
    y0 = ((y0 % tex.height) + tex.height) % tex.height;
    x1 = ((x1 % tex.width) + tex.width) % tex.width;
    y1 = ((y1 % tex.height) + tex.height) % tex.height;

    float tx = fx - std::floor(fx);
    float ty = fy - std::floor(fy);

    auto getPixel = [&](int px, int py) -> RGBA {
        size_t idx = (static_cast<size_t>(py) * tex.width + px) * 4;
        return {tex.pixels[idx + 0], tex.pixels[idx + 1], tex.pixels[idx + 2], tex.pixels[idx + 3]};
    };

    RGBA c00 = getPixel(x0, y0);
    RGBA c10 = getPixel(x1, y0);
    RGBA c01 = getPixel(x0, y1);
    RGBA c11 = getPixel(x1, y1);

    auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t {
        return static_cast<uint8_t>(a + (b - a) * t);
    };

    RGBA top = {lerp(c00.r, c10.r, tx), lerp(c00.g, c10.g, tx),
                lerp(c00.b, c10.b, tx), lerp(c00.a, c10.a, tx)};
    RGBA bot = {lerp(c01.r, c11.r, tx), lerp(c01.g, c11.g, tx),
                lerp(c01.b, c11.b, tx), lerp(c01.a, c11.a, tx)};

    return {lerp(top.r, bot.r, ty), lerp(top.g, bot.g, ty),
            lerp(top.b, bot.b, ty), lerp(top.a, bot.a, ty)};
}

} // namespace mgd
