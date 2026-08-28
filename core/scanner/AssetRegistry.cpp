#include "AssetRegistry.h"
#include <functional>

namespace mgd {

uint32_t AssetRegistry::registerAsset(const FileInfo& file, const RawAnalysisResult& result) {
    uint32_t id = next_id++;

    RegisteredAsset asset;
    asset.file = file;
    asset.result = result;
    asset.type = result.detected_type;

    assets[id] = asset;
    return id;
}

bool AssetRegistry::hasAsset(const std::string& path) const {
    for (const auto& [id, asset] : assets) {
        if (asset.file.path == path) {
            return true;
        }
    }
    return false;
}

std::optional<RegisteredAsset> AssetRegistry::getAsset(uint32_t id) const {
    auto it = assets.find(id);
    if (it != assets.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<RegisteredAsset> AssetRegistry::getAssetsByType(AssetType type) const {
    std::vector<RegisteredAsset> results;
    for (const auto& [id, asset] : assets) {
        if (asset.type == type) {
            results.push_back(asset);
        }
    }
    return results;
}

size_t AssetRegistry::count() const {
    return assets.size();
}

void AssetRegistry::clear() {
    assets.clear();
    next_id = 1;
}

} // namespace mgd
