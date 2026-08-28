#pragma once

#include "VisibleSet.h"
#include "../mental_map/MentalMap.h"
#include "../camera/Camera.h"
#include "../collision/CollisionSystem.h"

namespace mgd {

class IVisibilitySystem {
public:
    virtual ~IVisibilitySystem() = default;
    virtual VisibleSet compute(const MentalMap& map, const Camera& camera, const CollisionSystem& collision) = 0;
};

} // namespace mgd
