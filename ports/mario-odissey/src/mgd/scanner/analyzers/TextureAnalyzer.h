#pragma once

#include "IAssetAnalyzer.h"

namespace mgd {

class TextureAnalyzer : public IAssetAnalyzer {
public:
    AssetType getSupportedType() const override { return AssetType::TEXTURE; }
    RawAnalysisResult analyze(const FileInfo& file) override;
    bool canHandle(const FileInfo& file) const override;
};

} // namespace mgd
