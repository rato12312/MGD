#include "Infector.h"

namespace mgd {

EntityRecord Infector::normalizeEntity(uint32_t id, const Vec3& pos, const AABB& bounds,
                                        uint32_t visual_ref, RegionID region) {
    EntityRecord rec;
    rec.entity_id = id;
    rec.resource_id = 0;
    rec.collision_id = 0;
    rec.position = pos;
    rec.rotation = {0.0f, 0.0f, 0.0f};
    rec.scale = {1.0f, 1.0f, 1.0f};
    rec.bounds = bounds;
    rec.region_id = region;
    rec.visual_ref = visual_ref;
    rec.parent_id = INVALID_ENTITY_ID;
    rec.flags = 0;
    return rec;
}

ResourceRecord Infector::normalizeResource(ResourceID id, const std::string& name,
                                            const std::string& path, uint64_t hash,
                                            const AABB& bounds) {
    ResourceRecord rec;
    rec.id = id;
    rec.name = name;
    rec.path = path;
    rec.hash = hash;
    rec.bounds = bounds;
    rec.triangle_count = 0;
    rec.vertex_count = 0;
    return rec;
}

CollisionRecord Infector::normalizeCollision(CollisionID id, ShapeType type,
                                              const AABB& bounds) {
    CollisionRecord rec;
    rec.id = id;
    rec.shape_type = type;
    rec.bounds = bounds;
    rec.center = bounds.center();
    rec.half_extents = bounds.extents();
    rec.radius = 0.0f;
    rec.height = 0.0f;

    if (type == ShapeType::SPHERE) {
        rec.radius = rec.half_extents.length();
    } else if (type == ShapeType::CAPSULE) {
        rec.radius = std::min(rec.half_extents.x, rec.half_extents.z);
        rec.height = rec.half_extents.y * 2.0f;
    }

    return rec;
}

ScanMaterialRecord Infector::normalizeMaterial(MaterialID id, const RGBA& base_color) {
    ScanMaterialRecord rec;
    rec.id = id;
    rec.base_color = base_color;
    rec.texture_id = 0;
    rec.roughness = 0.5f;
    rec.metallic = 0.0f;
    rec.alpha_mode = AlphaMode::OPAQUE;
    return rec;
}

TextureRecord Infector::normalizeTexture(TextureID id, const std::string& path,
                                          int w, int h) {
    TextureRecord rec;
    rec.id = id;
    rec.path = path;
    rec.width = w;
    rec.height = h;
    rec.channels = 4;
    rec.hash = 0;
    return rec;
}

} // namespace mgd
