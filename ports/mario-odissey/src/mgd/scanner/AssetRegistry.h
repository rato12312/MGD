#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "analyzers/IAssetAnalyzer.h"
#include "FileDiscovery.h"

namespace mgd {

struct RegisteredAsset {
    FileInfo file;
    RawAnalysisResult result;
    AssetType type = AssetType::UNKNOWN;
};

class AssetRegistry {
    std::unordered_map<uint32_t, RegisteredAsset> assets;
    uint32_t next_id = 1;

public:
    uint32_t registerAsset(const FileInfo& file, const RawAnalysisResult& result);
    bool hasAsset(const std::string& path) const;
    std::optional<RegisteredAsset> getAsset(uint32_t id) const;
    std::vector<RegisteredAsset> getAssetsByType(AssetType type) const;
    size_t count() const;
    void clear();
};

} // namespace mgd
