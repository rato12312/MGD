#pragma once

#include "shapes/CollisionShape.h"
#include <vector>

namespace mgd {

class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;

    virtual void insert(CollisionID id, const AABB& bounds) = 0;
    virtual void remove(CollisionID id) = 0;
    virtual void update(CollisionID id, const AABB& bounds) = 0;
    virtual std::vector<CollisionID> query(const AABB& area) const = 0;
    virtual std::vector<CollisionID> queryPoint(const Vec3& point) const = 0;
    virtual void clear() = 0;
    virtual size_t size() const = 0;
};

} // namespace mgd
