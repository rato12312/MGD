#include "PolygonConsultant.h"
#include <algorithm>

namespace mgd {

const std::vector<Polygon>& PolygonConsultant::queryRegion(RegionID region) const {
    static const std::vector<Polygon> kEmpty;
    if (!cache_) return kEmpty;
    return static_cast<const RegionPolygonCache*>(cache_)->getPolygons(region);
}

const std::vector<Polygon>& PolygonConsultant::queryByPosition(const Vec3& pos) const {
    static const std::vector<Polygon> kEmpty;
    if (!cache_) return kEmpty;
    RegionID region = ChunkManager::worldToRegionId(pos);
    return static_cast<const RegionPolygonCache*>(cache_)->getPolygons(region);
}

std::optional<Polygon> PolygonConsultant::findPolygon(PolygonID pid) const {
    if (!cache_) return std::nullopt;
    return cache_->findPolygon(pid);
}

const Polygon* PolygonConsultant::findPolygonPtr(PolygonID pid) const {
    if (!cache_) return nullptr;
    return cache_->findPolygonPtr(pid);
}

std::optional<AssetID> PolygonConsultant::resolveAssetId(PolygonID pid) const {
    if (!cache_) return std::nullopt;
    return cache_->getAssetId(pid);
}

std::optional<RegisteredAsset> PolygonConsultant::resolveAsset(PolygonID pid) const {
    if (!cache_ || !registry_) return std::nullopt;
    auto aid = cache_->getAssetId(pid);
    if (!aid) return std::nullopt;
    return registry_->getAsset(*aid);
}

std::optional<RegisteredAsset> PolygonConsultant::resolveAssetById(AssetID aid) const {
    if (!registry_) return std::nullopt;
    return registry_->getAsset(aid);
}

bool PolygonConsultant::insertPolygon(RegionID region, const Polygon& poly) {
    if (!cache_) return false;
    return cache_->insert(region, poly);
}

bool PolygonConsultant::insertPolygon(const Vec3& position, RegionID region, PolygonID pid, AssetID aid, uint32_t flags) {
    if (!cache_) return false;
    Polygon p;
    p.position = position;
    p.polygon_id = pid;
    p.asset_id = aid;
    p.flags = flags;
    return cache_->insert(region, p);
}

EntityID PolygonConsultant::feedMentalMap(MentalMap& map, PolygonID pid) const {
    if (!cache_) return INVALID_ENTITY_ID;
    const Polygon* p = cache_->findPolygonPtr(pid);
    if (!p) return INVALID_ENTITY_ID;
    MentalEntity e;
    e.id = static_cast<EntityID>(p->polygon_id); // reutiliza PolygonID como EntityID quando possível
    e.resource_id = p->asset_id;
    e.collision_id = p->asset_id;
    e.transform.position = p->position;
    e.bounds.aabb = AABB(p->position - Vec3(0.5f,0.5f,0.5f), p->position + Vec3(0.5f,0.5f,0.5f));
    e.region_id = ChunkManager::worldToRegionId(p->position);
    e.flags = p->flags;
    e.state = EntityState::ACTIVE;
    e.visibility = VisibilityState::UNCHECKED;
    return map.addEntity(std::move(e));
}

size_t PolygonConsultant::feedMentalMapRegion(MentalMap& map, RegionID region) const {
    if (!cache_) return 0;
    const auto& polys = cache_->getPolygons(region);
    size_t n = 0;
    for (auto& p : polys) {
        if (feedMentalMap(map, p.polygon_id) != INVALID_ENTITY_ID) ++n;
    }
    return n;
}

std::vector<PolygonConsultant::ScoredPolygon> PolygonConsultant::queryScored(const Vec3& cameraPos, RegionID region, float minScreenArea) const {
    std::vector<ScoredPolygon> out;
    if (!cache_) return out;
    const auto& polys = cache_->getPolygons(region);
    out.reserve(polys.size());
    for (auto& p : polys) {
        float dist = p.distanceTo(cameraPos);
        // screenArea estimado: área projetada ~ 1/(dist^2) * k (k=10000 para 800x600)
        // Pedra gigante longe vira quadradinho — LOD natural, não artificial
        float screenArea = 10000.0f / (dist * dist + 1.0f);
        if (screenArea < minScreenArea) continue; // cull por área de tela, não por tamanho do asset
        ScoredPolygon sp;
        sp.poly = &p;
        sp.distance = dist;
        sp.screenArea = screenArea;
        sp.flags = p.flags;
        out.push_back(sp);
    }
    std::sort(out.begin(), out.end(), [](const ScoredPolygon& a, const ScoredPolygon& b){ return a.distance < b.distance; });
    return out;
}

std::vector<PolygonConsultant::ScoredPolygon> PolygonConsultant::queryScoredByPosition(const Vec3& cameraPos, const Vec3& queryPos, float minScreenArea) const {
    if (!cache_) return {};
    RegionID region = ChunkManager::worldToRegionId(queryPos);
    return queryScored(cameraPos, region, minScreenArea);
}

uint64_t PolygonConsultant::hits() const { return cache_ ? cache_->hits() : 0; }
uint64_t PolygonConsultant::misses() const { return cache_ ? cache_->misses() : 0; }
void PolygonConsultant::resetStats() const { if (cache_) cache_->resetStats(); }

} // namespace mgd
