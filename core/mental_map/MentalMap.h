#pragma once

#include "Entity.h"
#include "Region.h"
#include "events/EventManager.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <optional>
#include <cmath>

namespace mgd {

class MentalMap {
public:
    MentalMap() = default;

    // CRUD
    EntityID addEntity(MentalEntity entity);
    bool removeEntity(EntityID id);
    std::optional<std::reference_wrapper<MentalEntity>> getEntity(EntityID id);
    std::optional<std::reference_wrapper<const MentalEntity>> getEntity(EntityID id) const;
    size_t entityCount() const;

    // Dynamic updates
    void setTransform(EntityID id, Transform t);
    void setState(EntityID id, EntityState s);
    void setVisibility(EntityID id, VisibilityState v);
    void setRegion(EntityID id, RegionID r);
    void setVisualRef(EntityID id, uint32_t ref);

    // Hierarchy
    void setParent(EntityID child, EntityID parent);
    void removeParent(EntityID child);
    std::vector<EntityID> getChildren(EntityID parent) const;
    std::optional<EntityID> getParent(EntityID child) const;
    std::vector<EntityID> getRootEntities() const;

    // Regions
    void addRegion(Region region);
    void loadRegion(RegionID id);
    void unloadRegion(RegionID id);
    std::optional<std::reference_wrapper<Region>> getRegion(RegionID id);
    std::vector<RegionID> getLoadedRegionIDs() const;
    std::vector<EntityID> getEntitiesInRegion(RegionID id) const;

    // Queries
    std::vector<EntityID> getEntitiesInRange(const Vec3& center, float radius) const;
    std::vector<EntityID> getActiveEntities() const;
    void clear();

    // Events access
    EventManager& getEventManager();

private:
    std::vector<MentalEntity> entities_;
    std::unordered_map<EntityID, size_t> index_;
    EntityID next_id_ = 1;

    std::vector<Region> regions_;
    std::unordered_map<RegionID, size_t> region_index_;

    EventManager events_;
};

} // namespace mgd
