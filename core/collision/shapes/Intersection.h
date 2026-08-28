#pragma once

#include "../../common/Vec3.h"
#include "../../common/AABB.h"
#include "../../common/Ray.h"
#include "../../common/Plane.h"
#include "../../common/Frustum.h"
#include <cmath>
#include <algorithm>

namespace mgd {

inline bool intersectAABB_AABB(const AABB& a, const AABB& b) {
    return a.intersects(b);
}

inline bool intersectSphere_Sphere(Vec3 ca, float ra, Vec3 cb, float rb) {
    float dist_sq = ca.distanceSqTo(cb);
    float radius_sum = ra + rb;
    return dist_sq <= radius_sum * radius_sum;
}

inline bool intersectAABB_Sphere(const AABB& box, Vec3 center, float radius) {
    float d = 0.0f;
    for (int i = 0; i < 3; ++i) {
        float v = (&center.x)[i];
        float lo = (&box.min.x)[i];
        float hi = (&box.max.x)[i];
        if (v < lo) d += (lo - v) * (lo - v);
        else if (v > hi) d += (v - hi) * (v - hi);
    }
    return d <= radius * radius;
}

inline bool intersectRay_AABB(const Ray& ray, const AABB& box, float& t) {
    auto result = ray.intersectAABB(box);
    if (result) {
        t = *result;
        return true;
    }
    return false;
}

inline bool intersectRay_Sphere(const Ray& ray, Vec3 center, float radius, float& t) {
    auto result = ray.intersectSphere(center, radius);
    if (result) {
        t = *result;
        return true;
    }
    return false;
}

inline bool intersectFrustum_AABB(const Frustum& f, const AABB& box) {
    return f.intersectsAABB(box);
}

inline bool intersectFrustum_Sphere(const Frustum& f, Vec3 center, float radius) {
    return f.intersectsSphere(center, radius);
}

inline bool pointInAABB(const Vec3& p, const AABB& box) {
    return box.containsPoint(p);
}

inline bool pointInSphere(const Vec3& p, Vec3 center, float radius) {
    return p.distanceSqTo(center) <= radius * radius;
}

inline float distancePoint_AABB(const Vec3& p, const AABB& box) {
    float dist_sq = 0.0f;
    for (int i = 0; i < 3; ++i) {
        float v = (&p.x)[i];
        float lo = (&box.min.x)[i];
        float hi = (&box.max.x)[i];
        if (v < lo) {
            float diff = lo - v;
            dist_sq += diff * diff;
        } else if (v > hi) {
            float diff = v - hi;
            dist_sq += diff * diff;
        }
    }
    return std::sqrt(dist_sq);
}

inline float distancePoint_Sphere(const Vec3& p, Vec3 center, float radius) {
    float dist = p.distanceTo(center);
    return std::max(0.0f, dist - radius);
}

} // namespace mgd
