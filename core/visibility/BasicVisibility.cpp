#include "BasicVisibility.h"
#include "../collision/CollisionSystem.h"
#include "../collision/ISpatialIndex.h"
#include "../mental_map/MentalMap.h"
#include "../camera/Camera.h"
#include <algorithm>
#include <chrono>

namespace mgd {

VisibleSet BasicVisibility::compute(const MentalMap& map, const Camera& camera, const CollisionSystem& collision) {
    auto start = std::chrono::high_resolution_clock::now();

    VisibleSet result;
    const Frustum& frustum = camera.getFrustum();
    const CameraState& cam_state = camera.getState();

    Mat4 inv_vp = camera.getViewProjectionMatrix().inverse().value_or(Mat4::identity());

    AABB frustum_aabb = AABB::invalid();
    for (int cz = 0; cz <= 1; ++cz) {
        for (int cy = 0; cy <= 1; ++cy) {
            for (int cx = 0; cx <= 1; ++cx) {
                float nx = cx * 2.0f - 1.0f;
                float ny = cy * 2.0f - 1.0f;
                float nz = cz * 2.0f - 1.0f;
                Vec4 corner = inv_vp.transformPoint(Vec4(nx, ny, nz, 1.0f));
                if (corner.w > 1e-8f) {
                    frustum_aabb = frustum_aabb.expanded(Vec3(corner.x / corner.w, corner.y / corner.w, corner.z / corner.w));
                }
            }
        }
    }

    std::vector<CollisionID> candidates = collision.getSpatialIndex().query(frustum_aabb);

    for (CollisionID cid : candidates) {
        ++result.total_considered;

        AABB world_aabb = collision.getWorldAABB(cid);

        if (!frustum.containsAABB(world_aabb)) {
            continue;
        }

        Vec3 center = world_aabb.center();
        float dist = cam_state.position.distanceTo(center);
        if (dist > cam_state.view_distance) {
            continue;
        }

        auto entity_opt = map.getEntity(cid);
        if (!entity_opt) continue;
        const MentalEntity& entity = entity_opt->get();
        if (entity.state != EntityState::ACTIVE && entity.state != EntityState::DIRTY) continue;

        VisibleEntity ve;
        ve.id = entity.id;
        ve.position = entity.transform.position;
        ve.bounds = entity.bounds.aabb;
        ve.visual_ref = entity.visual_ref;
        ve.material_id = entity.resource_id;
        ve.distance_to_camera = dist;
        ve.flags = entity.flags;

        result.entities.push_back(ve);
    }

    std::sort(result.entities.begin(), result.entities.end(),
        [](const VisibleEntity& a, const VisibleEntity& b) {
            return a.distance_to_camera < b.distance_to_camera;
        });

    result.total_visible = result.entities.size();

    auto end = std::chrono::high_resolution_clock::now();
    result.compute_time_ms = std::chrono::duration<float, std::milli>(end - start).count();

    return result;
}

} // namespace mgd
