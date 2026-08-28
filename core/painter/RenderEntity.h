#pragma once

#include "../common/Types.h"
#include "../common/AABB.h"
#include "../mental_map/Entity.h"

namespace mgd {

struct RenderEntity {
    EntityID id;
    Transform transform;
    MeshID mesh_id;
    MaterialID material_id;
    AABB bounds;
    uint32_t flags;
};

} // namespace mgd
