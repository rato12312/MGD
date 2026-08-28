#pragma once

#include "../ISpatialIndex.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace mgd {

class GridIndex : public ISpatialIndex {
public:
    struct IVec3 {
        int x, y, z;

        bool operator==(const IVec3& o) const {
            return x == o.x && y == o.y && z == o.z;
        }
    };

    struct IVec3Hash {
        size_t operator()(const IVec3& v) const {
            size_t h = 0;
            h ^= std::hash<int>()(v.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(v.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(v.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    GridIndex(float cellSize = 100.0f);

    void insert(CollisionID id, const AABB& bounds) override;
    void remove(CollisionID id) override;
    void update(CollisionID id, const AABB& bounds) override;
    std::vector<CollisionID> query(const AABB& area) const override;
    std::vector<CollisionID> queryPoint(const Vec3& point) const override;
    void clear() override;
    size_t size() const override;

private:
    float cell_size_;
    std::unordered_map<IVec3, std::vector<CollisionID>, IVec3Hash> cells_;
    std::unordered_map<CollisionID, AABB> bounds_map_;

    IVec3 hashPosition(const Vec3& pos) const;
    std::vector<IVec3> getOverlappingCells(const AABB& area) const;
    void insertIntoCells(CollisionID id, const AABB& bounds);
    void removeFromCells(CollisionID id, const AABB& bounds);
};

} // namespace mgd
