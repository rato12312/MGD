#pragma once

#include <memory>
#include <string>
#include <vector>
#include "GameIdentity.h"
#include "FileDiscovery.h"
#include "analyzers/IAssetAnalyzer.h"

namespace mgd {

class ISourceAdapter {
public:
    virtual ~ISourceAdapter() = default;
    virtual GameIdentity getIdentity() const = 0;
    virtual std::vector<std::string> getKnownExtensions() const = 0;
    virtual bool canHandle(const GameIdentity& identity) const = 0;
    virtual std::unique_ptr<IAssetAnalyzer> getAnalyzer(const FileInfo& file) = 0;
};

} // namespace mgd
