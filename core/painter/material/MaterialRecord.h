#pragma once

#include "../../common/Types.h"
#include "../../common/RGBA.h"

namespace mgd {

struct MaterialRecord {
    MaterialID id = INVALID_MATERIAL_ID;
    RGBA base_color = {128, 128, 128, 255};
    TextureID texture_id = INVALID_TEXTURE_ID;
    TextureID normal_map_id = INVALID_TEXTURE_ID;
    float roughness = 0.5f;
    float metallic = 0.0f;
    AlphaMode alpha_mode = AlphaMode::OPAQUE;
};

} // namespace mgd
