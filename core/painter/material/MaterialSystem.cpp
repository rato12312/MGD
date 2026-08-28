#include "MaterialSystem.h"

namespace mgd {

void MaterialSystem::store(const MaterialRecord& mat) {
    materials[mat.id] = mat;
}

const MaterialRecord* MaterialSystem::get(MaterialID id) const {
    auto it = materials.find(id);
    if (it != materials.end()) return &it->second;
    return nullptr;
}

MaterialRecord MaterialSystem::getDefault() const {
    MaterialRecord def;
    def.id = INVALID_MATERIAL_ID;
    def.base_color = {128, 128, 128, 255};
    def.texture_id = INVALID_TEXTURE_ID;
    def.normal_map_id = INVALID_TEXTURE_ID;
    def.roughness = 0.5f;
    def.metallic = 0.0f;
    def.alpha_mode = AlphaMode::OPAQUE;
    return def;
}

bool MaterialSystem::has(MaterialID id) const {
    return materials.find(id) != materials.end();
}

void MaterialSystem::clear() {
    materials.clear();
}

} // namespace mgd
