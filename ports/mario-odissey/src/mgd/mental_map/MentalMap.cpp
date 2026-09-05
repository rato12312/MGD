#include "MentalMap.h"

namespace mgd {

EntityID MentalMap::addEntity(MentalEntity entity) {
    if (entity.id == INVALID_ENTITY_ID) {
        entity.id = next_id_++;
    } else {
        next_id_ = std::max(next_id_, static_cast<EntityID>(entity.id + 1));
    }

    EntityID id = entity.id;
    size_t index = entities_.size();
    entities_.push_back(std::move(entity));
    index_[id] = index;

    events_.emit(EntityAddedEvent{id});
    return id;
}

bool MentalMap::removeEntity(EntityID id) {
    auto it = index_.find(id);
    if (it == index_.end()) {
        return false;
    }

    size_t idx = it->second;

    // Remove from parent's children list if it has a parent
    if (entities_[idx].parent_id != INVALID_ENTITY_ID) {
        removeParent(id);
    }

    // Remove this entity as parent from all children
    for (EntityID child_id : entities_[idx].children) {
        auto child_it = index_.find(child_id);
        if (child_it != index_.end()) {
            entities_[child_it->second].parent_id = INVALID_ENTITY_ID;
        }
    }

    // Remove from region
    if (entities_[idx].region_id != INVALID_REGION_ID) {
        auto rit = region_index_.find(entities_[idx].region_id);
        if (rit != region_index_.end()) {
            auto& entity_ids = regions_[rit->second].entity_ids;
            entity_ids.erase(
                std::remove(entity_ids.begin(), entity_ids.end(), id),
                entity_ids.end());
        }
    }

    // Swap-and-pop removal
    EntityID last_id = entities_.back().id;
    if (id != last_id) {
        entities_[idx] = std::move(entities_.back());
        index_[last_id] = idx;
    }
    entities_.pop_back();
    index_.erase(it);

    events_.emit(EntityRemovedEvent{id});
    return true;
}

std::optional<std::reference_wrapper<MentalEntity>> MentalMap::getEntity(EntityID id) {
    auto it = index_.find(id);
    if (it == index_.end()) {
        return std::nullopt;
    }
    return std::ref(entities_[it->second]);
}

std::optional<std::reference_wrapper<const MentalEntity>> MentalMap::getEntity(EntityID id) const {
    auto it = index_.find(id);
    if (it == index_.end()) {
        return std::nullopt;
    }
    return std::cref(entities_[it->second]);
}

size_t MentalMap::entityCount() const {
    return entities_.size();
}

// Dynamic updates

void MentalMap::setTransform(EntityID id, Transform t) {
    auto it = index_.find(id);
    if (it == index_.end()) return;

    Vec3 old_pos = entities_[it->second].transform.position;
    Vec3 new_pos = t.position;
    entities_[it->second].transform = t;
    events_.emit(EntityMovedEvent{id, old_pos, new_pos});
}

void MentalMap::setState(EntityID id, EntityState s) {
    auto it = index_.find(id);
    if (it == index_.end()) return;
    entities_[it->second].state = s;
    events_.emit(EntityUpdatedEvent{id});
}

void MentalMap::setVisibility(EntityID id, VisibilityState v) {
    auto it = index_.find(id);
    if (it == index_.end()) return;
    entities_[it->second].visibility = v;
    events_.emit(EntityUpdatedEvent{id});
}

void MentalMap::setRegion(EntityID id, RegionID r) {
    auto it = index_.find(id);
    if (it == index_.end()) return;

    MentalEntity& ent = entities_[it->second];

    // Remove from old region
    if (ent.region_id != INVALID_REGION_ID) {
        auto rit = region_index_.find(ent.region_id);
        if (rit != region_index_.end()) {
            auto& entity_ids = regions_[rit->second].entity_ids;
            entity_ids.erase(
                std::remove(entity_ids.begin(), entity_ids.end(), id),
                entity_ids.end());
        }
    }

    ent.region_id = r;

    // Add to new region
    if (r != INVALID_REGION_ID) {
        auto rit = region_index_.find(r);
        if (rit != region_index_.end()) {
            regions_[rit->second].entity_ids.push_back(id);
        }
    }

    events_.emit(EntityUpdatedEvent{id});
}

void MentalMap::setVisualRef(EntityID id, uint32_t ref) {
    auto it = index_.find(id);
    if (it == index_.end()) return;
    entities_[it->second].visual_ref = ref;
    events_.emit(EntityUpdatedEvent{id});
}

// Hierarchy

void MentalMap::setParent(EntityID child, EntityID parent) {
    auto child_it = index_.find(child);
    if (child_it == index_.end()) return;

    // Remove from old parent first
    EntityID old_parent = entities_[child_it->second].parent_id;
    if (old_parent != INVALID_ENTITY_ID) {
        auto pit = index_.find(old_parent);
        if (pit != index_.end()) {
            auto& siblings = entities_[pit->second].children;
            siblings.erase(
                std::remove(siblings.begin(), siblings.end(), child),
                siblings.end());
        }
    }

    entities_[child_it->second].parent_id = parent;

    // Add to new parent's children
    if (parent != INVALID_ENTITY_ID) {
        auto pit = index_.find(parent);
        if (pit != index_.end()) {
            entities_[pit->second].children.push_back(child);
        }
    }
}

void MentalMap::removeParent(EntityID child) {
    auto child_it = index_.find(child);
    if (child_it == index_.end()) return;

    EntityID old_parent = entities_[child_it->second].parent_id;
    if (old_parent == INVALID_ENTITY_ID) return;

    auto pit = index_.find(old_parent);
    if (pit != index_.end()) {
        auto& siblings = entities_[pit->second].children;
        siblings.erase(
            std::remove(siblings.begin(), siblings.end(), child),
            siblings.end());
    }

    entities_[child_it->second].parent_id = INVALID_ENTITY_ID;
}

std::vector<EntityID> MentalMap::getChildren(EntityID parent) const {
    auto it = index_.find(parent);
    if (it == index_.end()) return {};
    return entities_[it->second].children;
}

std::optional<EntityID> MentalMap::getParent(EntityID child) const {
    auto it = index_.find(child);
    if (it == index_.end()) return std::nullopt;
    EntityID pid = entities_[it->second].parent_id;
    if (pid == INVALID_ENTITY_ID) return std::nullopt;
    return pid;
}

std::vector<EntityID> MentalMap::getRootEntities() const {
    std::vector<EntityID> roots;
    for (const auto& ent : entities_) {
        if (ent.parent_id == INVALID_ENTITY_ID) {
            roots.push_back(ent.id);
        }
    }
    return roots;
}

// Regions

void MentalMap::addRegion(Region region) {
    RegionID id = region.id;
    size_t index = regions_.size();
    regions_.push_back(std::move(region));
    region_index_[id] = index;
}

void MentalMap::loadRegion(RegionID id) {
    auto it = region_index_.find(id);
    if (it == region_index_.end()) return;
    regions_[it->second].load_state = RegionLoadState::LOADED;
    events_.emit(RegionLoadedEvent{id});
}

void MentalMap::unloadRegion(RegionID id) {
    auto it = region_index_.find(id);
    if (it == region_index_.end()) return;
    regions_[it->second].load_state = RegionLoadState::UNLOADED;
    events_.emit(RegionUnloadedEvent{id});
}

std::optional<std::reference_wrapper<Region>> MentalMap::getRegion(RegionID id) {
    auto it = region_index_.find(id);
    if (it == region_index_.end()) return std::nullopt;
    return std::ref(regions_[it->second]);
}

std::vector<RegionID> MentalMap::getLoadedRegionIDs() const {
    std::vector<RegionID> result;
    for (const auto& region : regions_) {
        if (region.load_state == RegionLoadState::LOADED) {
            result.push_back(region.id);
        }
    }
    return result;
}

std::vector<EntityID> MentalMap::getEntitiesInRegion(RegionID id) const {
    auto it = region_index_.find(id);
    if (it == region_index_.end()) return {};
    return regions_[it->second].entity_ids;
}

// Queries

std::vector<EntityID> MentalMap::getEntitiesInRange(const Vec3& center, float radius) const {
    float radius_sq = radius * radius;
    std::vector<EntityID> result;
    for (const auto& ent : entities_) {
        Vec3 diff = ent.transform.position - center;
        if (diff.lengthSq() <= radius_sq) {
            result.push_back(ent.id);
        }
    }
    return result;
}

std::vector<EntityID> MentalMap::getActiveEntities() const {
    std::vector<EntityID> result;
    for (const auto& ent : entities_) {
        if (ent.isLoaded()) {
            result.push_back(ent.id);
        }
    }
    return result;
}

void MentalMap::clear() {
    entities_.clear();
    index_.clear();
    next_id_ = 1;
    regions_.clear();
    region_index_.clear();
    events_.clear();
}

EventManager& MentalMap::getEventManager() {
    return events_;
}

} // namespace mgd
