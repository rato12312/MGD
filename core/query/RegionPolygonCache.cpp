#include "RegionPolygonCache.h"

namespace mgd {

const std::vector<Polygon> RegionPolygonCache::kEmpty{};

void RegionPolygonCache::enforceBudget() {
    if (budget_ == 0) return;
    while (polygon_index_.size() >= budget_ && !region_order_.empty()) {
        RegionID oldest = region_order_.front();
        region_order_.pop_front();
        region_seen_.erase(oldest);
        if (region_to_polys_.find(oldest) == region_to_polys_.end()) continue;
        clearRegion(oldest);
        evictions_++;
    }
}

bool RegionPolygonCache::insert(RegionID region, const Polygon& poly) {
    if (poly.polygon_id == INVALID_POLYGON_ID) return false;
    if (polygon_index_.find(poly.polygon_id) != polygon_index_.end()) return false; // já existe
    if (region_seen_.insert(region).second) region_order_.push_back(region);
    enforceBudget();
    auto& vec = region_to_polys_[region];
    size_t idx = vec.size();
    vec.push_back(poly);
    polygon_index_[poly.polygon_id] = {region, idx};
    return true;
}

bool RegionPolygonCache::insert(RegionID region, Polygon&& poly) {
    if (poly.polygon_id == INVALID_POLYGON_ID) return false;
    if (polygon_index_.find(poly.polygon_id) != polygon_index_.end()) return false;
    if (region_seen_.insert(region).second) region_order_.push_back(region);
    enforceBudget();
    auto& vec = region_to_polys_[region];
    size_t idx = vec.size();
    vec.push_back(std::move(poly));
    polygon_index_[vec.back().polygon_id] = {region, idx};
    return true;
}

const std::vector<Polygon>& RegionPolygonCache::getPolygons(RegionID region) const {
    auto it = region_to_polys_.find(region);
    if (it == region_to_polys_.end()) {
        misses_++;
        return kEmpty;
    }
    hits_++;
    return it->second;
}

std::vector<Polygon>& RegionPolygonCache::getPolygons(RegionID region) {
    auto it = region_to_polys_.find(region);
    if (it == region_to_polys_.end()) {
        misses_++;
        return region_to_polys_[region];
    }
    hits_++;
    return it->second;
}

std::optional<Polygon> RegionPolygonCache::findPolygon(PolygonID pid) const {
    auto it = polygon_index_.find(pid);
    if (it == polygon_index_.end()) {
        misses_++;
        return std::nullopt;
    }
    const Loc& loc = it->second;
    auto rit = region_to_polys_.find(loc.region);
    if (rit == region_to_polys_.end() || loc.idx >= rit->second.size()) {
        misses_++;
        return std::nullopt;
    }
    hits_++;
    return rit->second[loc.idx];
}

const Polygon* RegionPolygonCache::findPolygonPtr(PolygonID pid) const {
    auto it = polygon_index_.find(pid);
    if (it == polygon_index_.end()) {
        misses_++;
        return nullptr;
    }
    auto rit = region_to_polys_.find(it->second.region);
    if (rit == region_to_polys_.end() || it->second.idx >= rit->second.size()) {
        misses_++;
        return nullptr;
    }
    hits_++;
    return &rit->second[it->second.idx];
}

std::optional<AssetID> RegionPolygonCache::getAssetId(PolygonID pid) const {
    auto* p = findPolygonPtr(pid);
    if (!p) return std::nullopt;
    return p->asset_id;
}

bool RegionPolygonCache::removePolygon(PolygonID pid) {
    auto it = polygon_index_.find(pid);
    if (it == polygon_index_.end()) return false;
    Loc loc = it->second;
    auto rit = region_to_polys_.find(loc.region);
    if (rit == region_to_polys_.end()) return false;
    auto& vec = rit->second;
    // swap-erase para manter vetor contíguo sem buracos
    size_t lastIdx = vec.size() - 1;
    if (loc.idx != lastIdx) {
        vec[loc.idx] = std::move(vec[lastIdx]);
        polygon_index_[vec[loc.idx].polygon_id] = {loc.region, loc.idx};
    }
    vec.pop_back();
    polygon_index_.erase(it);
    if (vec.empty()) region_to_polys_.erase(rit);
    return true;
}

void RegionPolygonCache::clearRegion(RegionID region) {
    auto it = region_to_polys_.find(region);
    if (it == region_to_polys_.end()) return;
    for (auto& p : it->second) polygon_index_.erase(p.polygon_id);
    region_to_polys_.erase(it);
    region_seen_.erase(region);
}

void RegionPolygonCache::clear() {
    region_to_polys_.clear();
    polygon_index_.clear();
    region_order_.clear();
    region_seen_.clear();
    hits_ = misses_ = 0;
}

size_t RegionPolygonCache::regionCount() const { return region_to_polys_.size(); }
size_t RegionPolygonCache::polygonCount() const { return polygon_index_.size(); }
size_t RegionPolygonCache::polygonCount(RegionID region) const {
    auto it = region_to_polys_.find(region);
    return it == region_to_polys_.end() ? 0 : it->second.size();
}

} // namespace mgd
