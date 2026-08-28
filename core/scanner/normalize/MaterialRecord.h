#pragma once

#include <cstdint>
#include "../../common/Types.h"
#include "../../common/RGBA.h"

namespace mgd {

struct ScanMaterialRecord {
    MaterialID id = 0;
    RGBA base_color = {128, 128, 128, 255};
    TextureID texture_id = 0;
    float roughness = 0.5f;
    float metallic = 0.0f;
    AlphaMode alpha_mode = AlphaMode::OPAQUE;
};

} // namespace mgd
