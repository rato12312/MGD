#pragma once

#include "../common/Types.h"
#include "../common/AABB.h"
#include <vector>

namespace mgd {

struct Region {
    RegionID id = INVALID_REGION_ID;
    AABB bounds;
    RegionLoadState load_state = RegionLoadState::UNLOADED;
    std::vector<EntityID> entity_ids;

    size_t entity_count() const { return entity_ids.size(); }
};

} // namespace mgd
