#pragma once

#include "IAssetAnalyzer.h"

namespace mgd {

class BSAAnalyzer : public IAssetAnalyzer {
public:
    AssetType getSupportedType() const override { return AssetType::ARCHIVE; }
    RawAnalysisResult analyze(const FileInfo& file) override;
    bool canHandle(const FileInfo& file) const override;
};

} // namespace mgd
