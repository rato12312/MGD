#include "CollisionSystem.h"
#include <cmath>
#include <algorithm>

namespace mgd {

CollisionSystem::CollisionSystem() {}

void CollisionSystem::addShape(CollisionID id, const CollisionShapeData& shape) {
    shapes_[id] = shape;

    auto tit = transforms_.find(id);
    if (tit == transforms_.end()) {
        Transform t;
        t.position = Vec3(0, 0, 0);
        t.rotation_euler = Vec3(0, 0, 0);
        t.scale = Vec3(1, 1, 1);
        transforms_[id] = t;
    }

    AABB world_aabb = computeWorldAABB(shape, transforms_[id]);
    spatial_index_.insert(id, world_aabb);
}

void CollisionSystem::removeShape(CollisionID id) {
    spatial_index_.remove(id);
    shapes_.erase(id);
    transforms_.erase(id);

    for (auto it = entity_to_collision_.begin(); it != entity_to_collision_.end(); ) {
        if (it->second == id) {
            it = entity_to_collision_.erase(it);
        } else {
            ++it;
        }
    }
}

void CollisionSystem::updateTransform(CollisionID id, const Transform& t) {
    auto sit = shapes_.find(id);
    if (sit == shapes_.end()) return;

    transforms_[id] = t;
    AABB world_aabb = computeWorldAABB(sit->second, t);
    spatial_index_.update(id, world_aabb);
}

CollisionShapeData CollisionSystem::getShape(CollisionID id) const {
    auto it = shapes_.find(id);
    if (it == shapes_.end()) return {};
    return it->second;
}

AABB CollisionSystem::getWorldAABB(CollisionID id) const {
    auto sit = shapes_.find(id);
    if (sit == shapes_.end()) return AABB::invalid();

    auto tit = transforms_.find(id);
    if (tit == transforms_.end()) return AABB::invalid();

    return computeWorldAABB(sit->second, tit->second);
}

void CollisionSystem::syncFromMentalMap(MentalMap& map) {
    map.getEventManager().subscribe<EntityAddedEvent>([this, &map](const EntityAddedEvent& e) {
        onEntityAdded(e.id, map);
    });

    map.getEventManager().subscribe<EntityRemovedEvent>([this](const EntityRemovedEvent& e) {
        onEntityRemoved(e.id);
    });

    map.getEventManager().subscribe<EntityMovedEvent>([this, &map](const EntityMovedEvent& e) {
        onEntityMoved(e.id, map);
    });

    auto active = map.getActiveEntities();
    for (EntityID eid : active) {
        auto ent = map.getEntity(eid);
        if (ent && ent->get().collision_id != INVALID_COLLISION_ID) {
            onEntityAdded(eid, map);
        }
    }
}

ISpatialIndex& CollisionSystem::getSpatialIndex() {
    return spatial_index_;
}

const ISpatialIndex& CollisionSystem::getSpatialIndex() const {
    return spatial_index_;
}

std::vector<CollisionID> CollisionSystem::queryAABB(const AABB& area) const {
    return spatial_index_.query(area);
}

std::vector<CollisionID> CollisionSystem::querySphere(const Vec3& center, float radius) const {
    AABB sphere_aabb = AABB::fromCenterExtents(center, Vec3(radius, radius, radius));
    std::vector<CollisionID> candidates = spatial_index_.query(sphere_aabb);

    std::vector<CollisionID> result;
    for (CollisionID id : candidates) {
        auto sit = shapes_.find(id);
        if (sit == shapes_.end()) continue;

        auto tit = transforms_.find(id);
        if (tit == transforms_.end()) continue;

        AABB world_aabb = computeWorldAABB(sit->second, tit->second);
        if (world_aabb.overlapsSphere(center, radius)) {
            result.push_back(id);
        }
    }
    return result;
}

void CollisionSystem::onEntityMoved(EntityID entity_id, MentalMap& map) {
    auto eit = entity_to_collision_.find(entity_id);
    if (eit == entity_to_collision_.end()) return;

    CollisionID cid = eit->second;
    auto ent = map.getEntity(entity_id);
    if (!ent) return;

    Transform world_t = ent->get().transform;
    transforms_[cid] = world_t;

    auto sit = shapes_.find(cid);
    if (sit != shapes_.end()) {
        AABB world_aabb = computeWorldAABB(sit->second, world_t);
        spatial_index_.update(cid, world_aabb);
    }
}

void CollisionSystem::onEntityAdded(EntityID entity_id, MentalMap& map) {
    auto ent = map.getEntity(entity_id);
    if (!ent) return;

    const MentalEntity& me = ent->get();
    if (me.collision_id == INVALID_COLLISION_ID) return;

    CollisionID cid = me.collision_id;
    entity_to_collision_[entity_id] = cid;

    Transform world_t = me.transform;
    transforms_[cid] = world_t;

    if (shapes_.find(cid) == shapes_.end()) {
        CollisionShapeData shape;
        shape.id = cid;
        shape.type = ShapeType::AABB;
        shape.center = Vec3(0, 0, 0);
        shape.half_extents = me.bounds.aabb.extents();
        shape.radius = 0;
        shape.height = 0;
        shapes_[cid] = shape;
    }

    AABB world_aabb = computeWorldAABB(shapes_[cid], world_t);
    spatial_index_.insert(cid, world_aabb);
}

void CollisionSystem::onEntityRemoved(EntityID entity_id) {
    auto eit = entity_to_collision_.find(entity_id);
    if (eit == entity_to_collision_.end()) return;

    CollisionID cid = eit->second;
    spatial_index_.remove(cid);
    shapes_.erase(cid);
    transforms_.erase(cid);
    entity_to_collision_.erase(eit);
}

AABB CollisionSystem::computeWorldAABB(const CollisionShapeData& shape, const Transform& t) const {
    Vec3 world_center = t.position + shape.center;

    switch (shape.type) {
        case ShapeType::AABB: {
            Vec3 ext = Vec3(
                shape.half_extents.x * std::abs(t.scale.x),
                shape.half_extents.y * std::abs(t.scale.y),
                shape.half_extents.z * std::abs(t.scale.z)
            );
            return AABB(world_center - ext, world_center + ext);
        }
        case ShapeType::SPHERE: {
            float max_scale = std::max({std::abs(t.scale.x), std::abs(t.scale.y), std::abs(t.scale.z)});
            float r = shape.radius * max_scale;
            return AABB(world_center - Vec3(r, r, r), world_center + Vec3(r, r, r));
        }
        case ShapeType::CAPSULE: {
            float max_scale = std::max({std::abs(t.scale.x), std::abs(t.scale.y), std::abs(t.scale.z)});
            float r = shape.radius * max_scale;
            float h = shape.height * std::abs(t.scale.y);
            Vec3 ext(r, h * 0.5f + r, r);
            return AABB(world_center - ext, world_center + ext);
        }
        case ShapeType::OBB: {
            Vec3 he = Vec3(
                shape.half_extents.x * std::abs(t.scale.x),
                shape.half_extents.y * std::abs(t.scale.y),
                shape.half_extents.z * std::abs(t.scale.z)
            );

            float cx = std::cos(shape.obb_rotation.x);
            float sx = std::sin(shape.obb_rotation.x);
            float cy = std::cos(shape.obb_rotation.y);
            float sy = std::sin(shape.obb_rotation.y);
            float cz = std::cos(shape.obb_rotation.z);
            float sz = std::sin(shape.obb_rotation.z);

            float r00 = cy * cz;
            float r01 = -cy * sz;
            float r02 = sy;
            float r10 = sx * sy * cz + cx * sz;
            float r11 = -sx * sy * sz + cx * cz;
            float r12 = -sx * cy;
            float r20 = -cx * sy * cz + sx * sz;
            float r21 = cx * sy * sz + sx * cz;
            float r22 = cx * cy;

            float ex = std::abs(r00) * he.x + std::abs(r01) * he.y + std::abs(r02) * he.z;
            float ey = std::abs(r10) * he.x + std::abs(r11) * he.y + std::abs(r12) * he.z;
            float ez = std::abs(r20) * he.x + std::abs(r21) * he.y + std::abs(r22) * he.z;

            Vec3 ext(ex, ey, ez);
            return AABB(world_center - ext, world_center + ext);
        }
    }

    return AABB(world_center - Vec3(1, 1, 1), world_center + Vec3(1, 1, 1));
}

} // namespace mgd
