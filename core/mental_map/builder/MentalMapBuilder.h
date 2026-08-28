#pragma once

#include "../Entity.h"
#include "../Region.h"
#include "../MentalMap.h"
#include "../../scanner/normalize/EntityRecord.h"
#include <vector>

namespace mgd {

class MentalMapBuilder {
    MentalMap& mental_map;

public:
    MentalMapBuilder(MentalMap& map);

    void buildFromRecords(const std::vector<EntityRecord>& records);
    void updateFromRecord(const EntityRecord& record);
    void removeByEntityID(EntityID id);
};

} // namespace mgd