#pragma once

#include "shapes/CollisionShape.h"
#include "spatial/GridIndex.h"
#include "ISpatialIndex.h"
#include "../mental_map/MentalMap.h"
#include "../mental_map/events/MentalMapEvents.h"
#include <unordered_map>

namespace mgd {

class CollisionSystem {
public:
    CollisionSystem();

    void addShape(CollisionID id, const CollisionShapeData& shape);
    void removeShape(CollisionID id);
    void updateTransform(CollisionID id, const Transform& t);
    CollisionShapeData getShape(CollisionID id) const;
    AABB getWorldAABB(CollisionID id) const;

    void syncFromMentalMap(MentalMap& map);

    ISpatialIndex& getSpatialIndex();
    const ISpatialIndex& getSpatialIndex() const;

    std::vector<CollisionID> queryAABB(const AABB& area) const;
    std::vector<CollisionID> querySphere(const Vec3& center, float radius) const;

    void onEntityMoved(EntityID entity_id, MentalMap& map);
    void onEntityAdded(EntityID entity_id, MentalMap& map);
    void onEntityRemoved(EntityID entity_id);

private:
    std::unordered_map<CollisionID, CollisionShapeData> shapes_;
    std::unordered_map<CollisionID, Transform> transforms_;
    std::unordered_map<EntityID, CollisionID> entity_to_collision_;
    GridIndex spatial_index_;

    AABB computeWorldAABB(const CollisionShapeData& shape, const Transform& t) const;
};

} // namespace mgd
