#pragma once

#include "IAssetAnalyzer.h"

namespace mgd {

class MeshAnalyzer : public IAssetAnalyzer {
public:
    AssetType getSupportedType() const override { return AssetType::MESH; }
    RawAnalysisResult analyze(const FileInfo& file) override;
    bool canHandle(const FileInfo& file) const override;
};

} // namespace mgd
