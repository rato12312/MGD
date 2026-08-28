#pragma once

#include "Light.h"
#include "../material/MaterialRecord.h"
#include <vector>

namespace mgd {

class LightSystem {
    std::vector<Light> lights;

public:
    void addLight(const Light& light);
    void clearLights();
    RGBA calculateLighting(const Vec3& normal, const MaterialRecord& mat) const;
    const std::vector<Light>& getLights() const;
};

} // namespace mgd
