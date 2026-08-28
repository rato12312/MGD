#pragma once

#include "Vec3.h"
#include <cmath>

namespace mgd {

struct Plane {
    Vec3 normal;
    float d = 0.0f;

    Plane() = default;
    Plane(const Vec3& n, float d_) : normal(n.normalized()), d(d_) {}
    Plane(const Vec3& a, const Vec3& b, const Vec3& c) {
        normal = (b - a).cross(c - a).normalized();
        d = -normal.dot(a);
    }

    float signedDistance(const Vec3& p) const {
        return normal.dot(p) + d;
    }

    bool isFrontFacing(const Vec3& dir) const {
        return normal.dot(dir) < 0.0f;
    }

    Vec3 projectPoint(const Vec3& p) const {
        return p - normal * signedDistance(p);
    }

    Plane normalized() const {
        float len = normal.length();
        if (len < 1e-8f) return *this;
        return {normal / len, d / len};
    }
};

} // namespace mgd
