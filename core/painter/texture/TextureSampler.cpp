#include "TextureSampler.h"
#include <cmath>
#include <algorithm>

namespace mgd {

namespace {

int modFloor(int a, int size) {
    int r = a % size;
    return r < 0 ? r + size : r;
}

int clampTexel(int a, int size) {
    return a < 0 ? 0 : (a >= size ? size - 1 : a);
}

int resolveTexel(int a, int size, WrapMode mode) {
    return mode == WrapMode::REPEAT ? modFloor(a, size) : clampTexel(a, size);
}

} // namespace

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

    auto getPixel = [&](int px, int py) -> RGBA {
        size_t idx = (static_cast<size_t>(py) * tex.width + px) * 4;
        return {tex.pixels[idx + 0], tex.pixels[idx + 1], tex.pixels[idx + 2], tex.pixels[idx + 3]};
    };

    if (tex.sampler_state.filter == TextureFilter::NEAREST) {
        int px = resolveTexel(static_cast<int>(u * tex.width), tex.width, tex.sampler_state.wrap_u);
        int py = resolveTexel(static_cast<int>(v * tex.height), tex.height, tex.sampler_state.wrap_v);
        return getPixel(px, py);
    }

    float fx = u * tex.width - 0.5f;
    float fy = v * tex.height - 0.5f;

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // Resolve neighbours according to wrap mode: repeat must wrap, clamp must clamp
    // (no seamless bleeding across the CLAMP edge).
    x0 = resolveTexel(x0, tex.width, tex.sampler_state.wrap_u);
    y0 = resolveTexel(y0, tex.height, tex.sampler_state.wrap_v);
    x1 = resolveTexel(x1, tex.width, tex.sampler_state.wrap_u);
    y1 = resolveTexel(y1, tex.height, tex.sampler_state.wrap_v);

    float tx = fx - std::floor(fx);
    float ty = fy - std::floor(fy);

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
