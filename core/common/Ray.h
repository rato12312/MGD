#pragma once

#include "Vec3.h"
#include "AABB.h"
#include <optional>

namespace mgd {

struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray() = default;
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d.normalized()) {}

    Vec3 pointAt(float t) const { return origin + direction * t; }

    std::optional<float> intersectAABB(const AABB& box) const {
        float tmin = -1e30f;
        float tmax = 1e30f;

        for (int i = 0; i < 3; ++i) {
            float o = (&origin.x)[i];
            float d = (&direction.x)[i];
            float bmin = (&box.min.x)[i];
            float bmax = (&box.max.x)[i];

            if (std::abs(d) < 1e-8f) {
                if (o < bmin || o > bmax) return std::nullopt;
            } else {
                float invD = 1.0f / d;
                float t0 = (bmin - o) * invD;
                float t1 = (bmax - o) * invD;
                if (t0 > t1) std::swap(t0, t1);
                tmin = std::max(tmin, t0);
                tmax = std::min(tmax, t1);
                if (tmin > tmax) return std::nullopt;
            }
        }

        if (tmax < 0.0f) return std::nullopt;
        return tmin >= 0.0f ? tmin : tmax;
    }

    std::optional<float> intersectSphere(const Vec3& center, float radius) const {
        Vec3 oc = origin - center;
        float a = direction.dot(direction);
        float b = 2.0f * oc.dot(direction);
        float c = oc.dot(oc) - radius * radius;
        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f) return std::nullopt;
        float sqrtD = std::sqrt(discriminant);
        float t = (-b - sqrtD) / (2.0f * a);
        if (t < 0.0f) t = (-b + sqrtD) / (2.0f * a);
        if (t < 0.0f) return std::nullopt;
        return t;
    }

    struct HitResult {
        float distance;
        Vec3 position;
        Vec3 normal;
    };

    std::optional<HitResult> hitAABB(const AABB& box) const {
        auto t = intersectAABB(box);
        if (!t) return std::nullopt;

        HitResult hit;
        hit.distance = *t;
        hit.position = pointAt(*t);

        Vec3 center = box.center();
        Vec3 ext = box.extents();
        Vec3 local = hit.position - center;

        float minDist = 1e30f;
        hit.normal = {0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            float dist = ext.x - std::abs((&local.x)[i]);
            if (dist < minDist) {
                minDist = dist;
                hit.normal = {0, 0, 0};
                (&hit.normal.x)[i] = (&local.x)[i] > 0 ? 1.0f : -1.0f;
            }
        }

        return hit;
    }

    std::optional<HitResult> hitSphere(const Vec3& center, float radius) const {
        auto t = intersectSphere(center, radius);
        if (!t) return std::nullopt;

        HitResult hit;
        hit.distance = *t;
        hit.position = pointAt(*t);
        hit.normal = (hit.position - center).normalized();
        return hit;
    }
};

} // namespace mgd
