#pragma once

#include "../common/Vec2.h"
#include "../common/Vec3.h"
#include "../common/RGBA.h"

namespace mgd {

struct RenderVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    RGBA color;
    float rhw = 1.0f;
};

} // namespace mgd
