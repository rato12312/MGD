#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "../FileDiscovery.h"
#include "../../common/Types.h"
#include "../../common/AABB.h"

namespace mgd {

enum class AssetType { UNKNOWN, MESH, TEXTURE, MATERIAL, WORLD, COLLISION, ARCHIVE, SCRIPT, METADATA };

struct RawAnalysisResult {
    FileInfo file;
    AssetType detected_type = AssetType::UNKNOWN;
    std::map<std::string, std::string> metadata;
    std::vector<uint32_t> dependency_ids;
    AABB spatial_bounds;
    bool success = false;
    std::string error;
};

class IAssetAnalyzer {
public:
    virtual ~IAssetAnalyzer() = default;
    virtual AssetType getSupportedType() const = 0;
    virtual RawAnalysisResult analyze(const FileInfo& file) = 0;
    virtual bool canHandle(const FileInfo& file) const = 0;
};

} // namespace mgd
