#pragma once

#include "../../common/Types.h"
#include "../../common/Vec3.h"
#include "../../common/AABB.h"

namespace mgd {

struct CollisionShapeData {
    CollisionID id = INVALID_COLLISION_ID;
    ShapeType type = ShapeType::AABB;
    Vec3 center;
    Vec3 half_extents;
    float radius = 0.0f;
    float height = 0.0f;
    Vec3 obb_rotation;
};

} // namespace mgd
