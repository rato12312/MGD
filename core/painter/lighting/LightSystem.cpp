#include "LightSystem.h"
#include <algorithm>
#include <cmath>

namespace mgd {

void LightSystem::addLight(const Light& light) {
    lights.push_back(light);
}

void LightSystem::clearLights() {
    lights.clear();
}

RGBA LightSystem::calculateLighting(const Vec3& normal, const MaterialRecord& mat) const {
    float ambientStrength = 0.15f;
    float r = mat.base_color.r * ambientStrength;
    float g = mat.base_color.g * ambientStrength;
    float b = mat.base_color.b * ambientStrength;

    Vec3 n = normal.normalized();

    for (const auto& light : lights) {
        if (light.type == LightType::DIRECTIONAL) {
            Vec3 lightDir = light.direction.normalized();
            float diff = std::max(0.0f, n.dot(-lightDir));
            r += mat.base_color.r * diff * (light.color.r / 255.0f) * light.intensity;
            g += mat.base_color.g * diff * (light.color.g / 255.0f) * light.intensity;
            b += mat.base_color.b * diff * (light.color.b / 255.0f) * light.intensity;
        } else if (light.type == LightType::POINT) {
            // Placeholder: simplified point light
            float dist = 1.0f; // Would need world position of fragment
            float attenuation = 1.0f / (1.0f + 0.1f * dist + 0.01f * dist * dist);
            float diff = std::max(0.0f, n.dot(-light.direction.normalized()));
            r += mat.base_color.r * diff * (light.color.r / 255.0f) * light.intensity * attenuation;
            g += mat.base_color.g * diff * (light.color.g / 255.0f) * light.intensity * attenuation;
            b += mat.base_color.b * diff * (light.color.b / 255.0f) * light.intensity * attenuation;
        }
    }

    uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
    uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
    uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));

    return {cr, cg, cb, mat.base_color.a};
}

const std::vector<Light>& LightSystem::getLights() const {
    return lights;
}

} // namespace mgd
