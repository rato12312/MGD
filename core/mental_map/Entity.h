#pragma once

#include "../common/Types.h"
#include "../common/Vec3.h"
#include "../common/AABB.h"
#include <vector>

namespace mgd {

struct Transform {
    Vec3 position;
    Vec3 rotation_euler;
    Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
};

struct Bounds {
    AABB aabb;
};

struct MentalEntity {
    EntityID id = INVALID_ENTITY_ID;
    ResourceID resource_id = 0;
    CollisionID collision_id = INVALID_COLLISION_ID;
    Transform transform;
    Bounds bounds;
    EntityState state = EntityState::ACTIVE;
    VisibilityState visibility = VisibilityState::UNCHECKED;
    RegionID region_id = INVALID_REGION_ID;
    EntityID parent_id = INVALID_ENTITY_ID;
    uint32_t visual_ref = 0;
    uint32_t flags = 0;
    std::vector<EntityID> children;

    bool isVisible() const {
        return visibility == VisibilityState::VISIBLE;
    }

    bool isLoaded() const {
        return state == EntityState::ACTIVE || state == EntityState::DIRTY;
    }
};

} // namespace mgd
