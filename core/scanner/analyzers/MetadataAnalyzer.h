#pragma once

#include "IAssetAnalyzer.h"

namespace mgd {

class MetadataAnalyzer : public IAssetAnalyzer {
public:
    AssetType getSupportedType() const override { return AssetType::METADATA; }
    RawAnalysisResult analyze(const FileInfo& file) override;
    bool canHandle(const FileInfo& file) const override;
};

} // namespace mgd
