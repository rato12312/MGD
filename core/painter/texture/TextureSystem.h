#pragma once

#include "TextureData.h"
#include <unordered_map>

namespace mgd {

class TextureSystem {
    std::unordered_map<TextureID, TextureData> textures;

public:
    void store(const TextureData& tex);
    const TextureData* get(TextureID id) const;
    TextureData getDefault() const;
    bool has(TextureID id) const;
    void clear();
};

} // namespace mgd
