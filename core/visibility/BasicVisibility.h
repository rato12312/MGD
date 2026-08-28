#pragma once

#include "IVisibilitySystem.h"

namespace mgd {

class BasicVisibility : public IVisibilitySystem {
public:
    VisibleSet compute(const MentalMap& map, const Camera& camera, const CollisionSystem& collision) override;
};

} // namespace mgd
