#include "BasicVisibility.h"
#include "../collision/CollisionSystem.h"
#include "../collision/ISpatialIndex.h"
#include "../mental_map/MentalMap.h"
#include "../camera/Camera.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>

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
                float nz = static_cast<float>(cz); // NDC z in [0, 1] (D3D)
                Vec4 corner = inv_vp.transformPoint(Vec4(nx, ny, nz, 1.0f));
                if (corner.w > 1e-8f) {
                    frustum_aabb = frustum_aabb.expanded(Vec3(corner.x / corner.w, corner.y / corner.w, corner.z / corner.w));
                }
            }
        }
    }

    // Map collision ids (as returned by the spatial index) back to entities via
    // their collision_id, instead of assuming collision_id == entity_id.
    std::unordered_map<CollisionID, EntityID> entity_by_collision;
    for (EntityID eid : map.getActiveEntities()) {
        auto e = map.getEntity(eid);
        if (e && e->get().collision_id != INVALID_COLLISION_ID) {
            entity_by_collision[e->get().collision_id] = eid;
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

        auto eit = entity_by_collision.find(cid);
        if (eit == entity_by_collision.end()) continue;
        auto entity_opt = map.getEntity(eit->second);
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
        ve.scale = entity.transform.scale;

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
