#include "Raycast.h"
#include "../shapes/Intersection.h"
#include <algorithm>

namespace mgd {

RayHit RaycastSystem::raycast(const Ray& ray, float max_dist, const CollisionSystem& collision) {
    RayHit closest;
    closest.hit = false;
    closest.distance = max_dist;

    AABB ray_bounds = AABB::fromCenterExtents(
        ray.origin + ray.direction * (max_dist * 0.5f),
        Vec3(max_dist * 0.5f, max_dist * 0.5f, max_dist * 0.5f)
    );

    auto candidates = collision.getSpatialIndex().query(ray_bounds);

    for (CollisionID id : candidates) {
        CollisionShapeData shape = collision.getShape(id);
        AABB world_aabb = collision.getWorldAABB(id);

        float t = 0.0f;
        bool hit = false;

        switch (shape.type) {
            case ShapeType::SPHERE: {
                Vec3 world_center = world_aabb.center();
                float half = world_aabb.extents().x;
                hit = intersectRay_Sphere(ray, world_center, half, t);
                break;
            }
            case ShapeType::AABB:
            case ShapeType::OBB:
            case ShapeType::CAPSULE:
            default:
                hit = intersectRay_AABB(ray, world_aabb, t);
                break;
        }

        if (hit && t < closest.distance && t >= 0.0f) {
            closest.hit_id = id;
            closest.distance = t;
            closest.position = ray.pointAt(t);

            if (shape.type == ShapeType::SPHERE) {
                closest.normal = (closest.position - world_aabb.center()).normalized();
            } else {
                Vec3 aabb_center = world_aabb.center();
                Vec3 ext = world_aabb.extents();
                Vec3 local = closest.position - aabb_center;

                float min_dist = 1e30f;
                closest.normal = Vec3(0, 0, 0);
                for (int i = 0; i < 3; ++i) {
                    float d = (&ext.x)[i] - std::abs((&local.x)[i]);
                    if (d < min_dist) {
                        min_dist = d;
                        closest.normal = Vec3(0, 0, 0);
                        (&closest.normal.x)[i] = (&local.x)[i] > 0 ? 1.0f : -1.0f;
                    }
                }
            }

            closest.hit = true;
        }
    }

    return closest;
}

} // namespace mgd
