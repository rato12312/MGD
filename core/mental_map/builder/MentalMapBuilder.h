#pragma once

#include "../Entity.h"
#include "../Region.h"
#include "../MentalMap.h"
#include <vector>

namespace mgd {

struct CacheRecordData {
    uint32_t entity_id;
    uint32_t resource_id;
    uint32_t collision_id;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    AABB bounds;
    RegionID region_id;
    uint32_t visual_ref;
    EntityID parent_id;
    uint32_t flags;
};

class MentalMapBuilder {
    MentalMap& mental_map;

public:
    MentalMapBuilder(MentalMap& map);

    void buildFromCacheRecords(const std::vector<CacheRecordData>& records);
    void updateFromCacheRecord(const CacheRecordData& record);
    void removeByEntityID(EntityID id);
};

} // namespace mgd
