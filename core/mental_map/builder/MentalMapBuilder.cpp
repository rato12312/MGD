#include "MentalMapBuilder.h"

namespace mgd {

MentalMapBuilder::MentalMapBuilder(MentalMap& map)
    : mental_map(map) {}

void MentalMapBuilder::buildFromRecords(const std::vector<EntityRecord>& records) {
    mental_map.clear();

    for (const auto& rec : records) {
        MentalEntity entity;
        entity.id = static_cast<EntityID>(rec.entity_id);
        entity.resource_id = static_cast<ResourceID>(rec.resource_id);
        entity.collision_id = static_cast<CollisionID>(rec.collision_id);
        entity.transform.position = rec.position;
        entity.transform.rotation_euler = rec.rotation;
        entity.transform.scale = rec.scale;
        entity.bounds.aabb = rec.bounds;
        entity.region_id = rec.region_id;
        entity.visual_ref = rec.visual_ref;
        entity.parent_id = rec.parent_id;
        entity.flags = rec.flags;
        entity.state = EntityState::ACTIVE;
        entity.visibility = VisibilityState::UNCHECKED;

        mental_map.addEntity(entity);
    }

    for (const auto& rec : records) {
        if (rec.parent_id != INVALID_ENTITY_ID) {
            mental_map.setParent(static_cast<EntityID>(rec.entity_id), rec.parent_id);
        }
    }
}

void MentalMapBuilder::updateFromRecord(const EntityRecord& record) {
    EntityID id = static_cast<EntityID>(record.entity_id);
    auto existing = mental_map.getEntity(id);

    if (existing) {
        MentalEntity& ent = existing->get();
        ent.resource_id = static_cast<ResourceID>(record.resource_id);
        ent.collision_id = static_cast<CollisionID>(record.collision_id);
        ent.transform.position = record.position;
        ent.transform.rotation_euler = record.rotation;
        ent.transform.scale = record.scale;
        ent.bounds.aabb = record.bounds;
        ent.visual_ref = record.visual_ref;
        ent.flags = record.flags;
        ent.state = EntityState::DIRTY;

        if (ent.region_id != record.region_id) {
            mental_map.setRegion(id, record.region_id);
        }

        if (ent.parent_id != record.parent_id) {
            mental_map.setParent(id, record.parent_id);
        }
    } else {
        MentalEntity entity;
        entity.id = id;
        entity.resource_id = static_cast<ResourceID>(record.resource_id);
        entity.collision_id = static_cast<CollisionID>(record.collision_id);
        entity.transform.position = record.position;
        entity.transform.rotation_euler = record.rotation;
        entity.transform.scale = record.scale;
        entity.bounds.aabb = record.bounds;
        entity.region_id = record.region_id;
        entity.visual_ref = record.visual_ref;
        entity.parent_id = record.parent_id;
        entity.flags = record.flags;
        entity.state = EntityState::ACTIVE;
        entity.visibility = VisibilityState::UNCHECKED;

        EntityID assigned_id = mental_map.addEntity(entity);

        if (record.parent_id != INVALID_ENTITY_ID) {
            mental_map.setParent(assigned_id, record.parent_id);
        }
    }
}

void MentalMapBuilder::removeByEntityID(EntityID id) {
    mental_map.removeEntity(id);
}

} // namespace mgd