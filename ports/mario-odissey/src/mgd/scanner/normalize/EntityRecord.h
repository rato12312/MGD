#pragma once

#include <cstdint>
#include "../../common/Types.h"
#include "../../common/Vec3.h"
#include "../../common/AABB.h"

namespace mgd {

struct EntityRecord {
    uint32_t entity_id = 0;
    uint32_t resource_id = 0;
    uint32_t collision_id = 0;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    AABB bounds;
    RegionID region_id = INVALID_REGION_ID;
    uint32_t visual_ref = 0;
    EntityID parent_id = INVALID_ENTITY_ID;
    uint32_t flags = 0;
};

} // namespace mgd
