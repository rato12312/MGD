#pragma once

#include "Vec3.h"
#include <algorithm>
#include <cmath>

namespace mgd {

struct AABB {
    Vec3 min;
    Vec3 max;

    AABB() = default;
    AABB(const Vec3& min_, const Vec3& max_) : min(min_), max(max_) {}

    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 extents() const { return (max - min) * 0.5f; }
    Vec3 size() const { return max - min; }

    bool containsPoint(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }

    bool intersects(const AABB& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }

    bool containsSphere(const Vec3& center, float radius) const {
        float d = 0.0f;
        for (int i = 0; i < 3; ++i) {
            float v = (&center.x)[i];
            float lo = (&min.x)[i];
            float hi = (&max.x)[i];
            if (v < lo) d += (lo - v) * (lo - v);
            else if (v > hi) d += (v - hi) * (v - hi);
        }
        return d <= radius * radius;
    }

    bool overlapsSphere(const Vec3& center, float radius) const {
        float d = 0.0f;
        for (int i = 0; i < 3; ++i) {
            float v = (&center.x)[i];
            float lo = (&min.x)[i];
            float hi = (&max.x)[i];
            if (v < lo) d += (lo - v) * (lo - v);
            else if (v > hi) d += (v - hi) * (v - hi);
        }
        return d <= radius * radius;
    }

    AABB merge(const AABB& o) const {
        return {
            {std::min(min.x, o.min.x), std::min(min.y, o.min.y), std::min(min.z, o.min.z)},
            {std::max(max.x, o.max.x), std::max(max.y, o.max.y), std::max(max.z, o.max.z)}
        };
    }

    AABB expanded(const Vec3& point) const {
        return {
            {std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z)},
            {std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z)}
        };
    }

    AABB expandedBy(float amount) const {
        return {min - Vec3(amount, amount, amount), max + Vec3(amount, amount, amount)};
    }

    static AABB fromCenterExtents(const Vec3& center, const Vec3& extents) {
        return {center - extents, center + extents};
    }

    static AABB invalid() {
        return {
            {1e30f, 1e30f, 1e30f},
            {-1e30f, -1e30f, -1e30f}
        };
    }
};

} // namespace mgd
