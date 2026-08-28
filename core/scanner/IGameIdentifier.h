#pragma once

#include <string>
#include "GameIdentity.h"

namespace mgd {

class IGameIdentifier {
public:
    virtual ~IGameIdentifier() = default;
    virtual IdentificationResult identify(const std::string& source_root) = 0;
};

} // namespace mgd
