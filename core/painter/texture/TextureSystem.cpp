#include "TextureSystem.h"
#include <cmath>

namespace mgd {

void TextureSystem::store(const TextureData& tex) {
    textures[tex.id] = tex;
}

const TextureData* TextureSystem::get(TextureID id) const {
    auto it = textures.find(id);
    if (it != textures.end()) return &it->second;
    return nullptr;
}

TextureData TextureSystem::getDefault() const {
    constexpr int SIZE = 16;
    TextureData tex;
    tex.id = INVALID_TEXTURE_ID;
    tex.width = SIZE;
    tex.height = SIZE;
    tex.channels = 4;
    tex.pixels.resize(SIZE * SIZE * 4);
    tex.sampler_state.wrap_u = WrapMode::REPEAT;
    tex.sampler_state.wrap_v = WrapMode::REPEAT;
    tex.sampler_state.filter = TextureFilter::NEAREST;

    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            bool white = ((x / 4) + (y / 4)) % 2 == 0;
            uint8_t c = white ? 200 : 80;
            size_t idx = (static_cast<size_t>(y) * SIZE + x) * 4;
            tex.pixels[idx + 0] = c;
            tex.pixels[idx + 1] = c;
            tex.pixels[idx + 2] = c;
            tex.pixels[idx + 3] = 255;
        }
    }

    return tex;
}

bool TextureSystem::has(TextureID id) const {
    return textures.find(id) != textures.end();
}

void TextureSystem::clear() {
    textures.clear();
}

} // namespace mgd
