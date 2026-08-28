#pragma once

#include "../../common/Types.h"
#include "../../common/Vec3.h"
#include "../../common/RGBA.h"

namespace mgd {

struct Light {
    LightType type = LightType::DIRECTIONAL;
    Vec3 direction = {0.0f, -1.0f, 0.0f};
    Vec3 position = {0.0f, 0.0f, 0.0f};
    RGBA color = {255, 255, 255, 255};
    float intensity = 1.0f;
};

} // namespace mgd
