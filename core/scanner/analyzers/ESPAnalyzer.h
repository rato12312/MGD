#pragma once

#include "IAssetAnalyzer.h"

namespace mgd {

class ESPAnalyzer : public IAssetAnalyzer {
public:
    AssetType getSupportedType() const override { return AssetType::WORLD; }
    RawAnalysisResult analyze(const FileInfo& file) override;
    bool canHandle(const FileInfo& file) const override;
};

} // namespace mgd
