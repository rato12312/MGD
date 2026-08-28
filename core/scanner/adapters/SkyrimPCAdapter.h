#pragma once

#include "../ISourceAdapter.h"

namespace mgd {

class SkyrimPCAdapter : public ISourceAdapter {
public:
    GameIdentity getIdentity() const override;
    std::vector<std::string> getKnownExtensions() const override;
    bool canHandle(const GameIdentity& identity) const override;
    std::unique_ptr<IAssetAnalyzer> getAnalyzer(const FileInfo& file) override;
};

} // namespace mgd
