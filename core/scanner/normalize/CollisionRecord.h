#pragma once

#include <cstdint>
#include "../../common/Types.h"
#include "../../common/Vec3.h"
#include "../../common/AABB.h"

namespace mgd {

struct CollisionRecord {
    CollisionID id = 0;
    ShapeType shape_type = ShapeType::AABB;
    AABB bounds;
    Vec3 center;
    Vec3 half_extents;
    float radius = 0.0f;
    float height = 0.0f;
};

} // namespace mgd
