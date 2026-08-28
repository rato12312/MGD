#include "GridIndex.h"
#include <algorithm>
#include <cmath>

namespace mgd {

GridIndex::GridIndex(float cellSize)
    : cell_size_(cellSize) {}

GridIndex::IVec3 GridIndex::hashPosition(const Vec3& pos) const {
    return {
        static_cast<int>(std::floor(pos.x / cell_size_)),
        static_cast<int>(std::floor(pos.y / cell_size_)),
        static_cast<int>(std::floor(pos.z / cell_size_))
    };
}

std::vector<GridIndex::IVec3> GridIndex::getOverlappingCells(const AABB& area) const {
    IVec3 lo = hashPosition(area.min);
    IVec3 hi = hashPosition(area.max);

    std::vector<IVec3> cells;
    cells.reserve(static_cast<size_t>((hi.x - lo.x + 1)) *
                  static_cast<size_t>((hi.y - lo.y + 1)) *
                  static_cast<size_t>((hi.z - lo.z + 1)));

    for (int x = lo.x; x <= hi.x; ++x) {
        for (int y = lo.y; y <= hi.y; ++y) {
            for (int z = lo.z; z <= hi.z; ++z) {
                cells.push_back({x, y, z});
            }
        }
    }
    return cells;
}

void GridIndex::insertIntoCells(CollisionID id, const AABB& bounds) {
    auto cells = getOverlappingCells(bounds);
    for (auto& cell : cells) {
        cells_[cell].push_back(id);
    }
}

void GridIndex::removeFromCells(CollisionID id, const AABB& bounds) {
    auto cells = getOverlappingCells(bounds);
    for (auto& cell : cells) {
        auto it = cells_.find(cell);
        if (it != cells_.end()) {
            auto& vec = it->second;
            vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
            if (vec.empty()) {
                cells_.erase(it);
            }
        }
    }
}

void GridIndex::insert(CollisionID id, const AABB& bounds) {
    bounds_map_[id] = bounds;
    insertIntoCells(id, bounds);
}

void GridIndex::remove(CollisionID id) {
    auto it = bounds_map_.find(id);
    if (it == bounds_map_.end()) return;
    removeFromCells(id, it->second);
    bounds_map_.erase(it);
}

void GridIndex::update(CollisionID id, const AABB& bounds) {
    auto it = bounds_map_.find(id);
    if (it != bounds_map_.end()) {
        removeFromCells(id, it->second);
    }
    bounds_map_[id] = bounds;
    insertIntoCells(id, bounds);
}

std::vector<CollisionID> GridIndex::query(const AABB& area) const {
    auto cells = getOverlappingCells(area);
    std::vector<CollisionID> result;

    for (auto& cell : cells) {
        auto cit = cells_.find(cell);
        if (cit != cells_.end()) {
            for (CollisionID id : cit->second) {
                auto bit = bounds_map_.find(id);
                if (bit != bounds_map_.end()) {
                    if (bit->second.intersects(area)) {
                        result.push_back(id);
                    }
                }
            }
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<CollisionID> GridIndex::queryPoint(const Vec3& point) const {
    IVec3 cell = hashPosition(point);
    std::vector<CollisionID> result;

    auto it = cells_.find(cell);
    if (it != cells_.end()) {
        for (CollisionID id : it->second) {
            auto bit = bounds_map_.find(id);
            if (bit != bounds_map_.end()) {
                if (bit->second.containsPoint(point)) {
                    result.push_back(id);
                }
            }
        }
    }

    return result;
}

void GridIndex::clear() {
    cells_.clear();
    bounds_map_.clear();
}

size_t GridIndex::size() const {
    return bounds_map_.size();
}

} // namespace mgd
