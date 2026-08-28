#pragma once

#include "TextureData.h"
#include "../../common/RGBA.h"

namespace mgd {

class TextureSampler {
    static float wrapCoordinate(float coord, int size, WrapMode mode);

public:
    static RGBA sample(const TextureData& tex, float u, float v);
};

} // namespace mgd
