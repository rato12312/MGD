#pragma once

#include "../CollisionSystem.h"
#include "../../common/Ray.h"

namespace mgd {

struct RayHit {
    CollisionID hit_id = INVALID_COLLISION_ID;
    float distance = 0.0f;
    Vec3 position;
    Vec3 normal;
    bool hit = false;
};

class RaycastSystem {
public:
    RayHit raycast(const Ray& ray, float max_dist, const CollisionSystem& collision);
};

} // namespace mgd
