#pragma once

#include "../common/Vec3.h"
#include "../common/AABB.h"
#include "../mental_map/Entity.h"
#include <vector>

namespace mgd {

struct VisibleEntity {
    EntityID id;
    Vec3 position;
    AABB bounds;
    uint32_t visual_ref;
    uint32_t material_id;
    float distance_to_camera;
    uint32_t flags;
    Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
};

struct VisibleSet {
    std::vector<VisibleEntity> entities;
    size_t total_considered = 0;
    size_t total_visible = 0;
    float compute_time_ms = 0;
};

} // namespace mgd
