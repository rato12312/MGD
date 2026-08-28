#pragma once

#include "../../common/Types.h"
#include <vector>
#include <cstdint>

namespace mgd {

struct TextureSamplerState {
    WrapMode wrap_u = WrapMode::REPEAT;
    WrapMode wrap_v = WrapMode::REPEAT;
    TextureFilter filter = TextureFilter::NEAREST;
};

struct TextureData {
    TextureID id = INVALID_TEXTURE_ID;
    int width = 0;
    int height = 0;
    int channels = 4;
    std::vector<uint8_t> pixels;
    TextureSamplerState sampler_state;
};

} // namespace mgd
