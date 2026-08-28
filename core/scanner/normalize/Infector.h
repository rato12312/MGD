#pragma once

#include "EntityRecord.h"
#include "ResourceRecord.h"
#include "CollisionRecord.h"
#include "MaterialRecord.h"
#include "TextureRecord.h"
#include "../../common/Vec3.h"
#include "../../common/AABB.h"
#include "../../common/RGBA.h"
#include "../../common/Types.h"

namespace mgd {

class Infector {
public:
    EntityRecord normalizeEntity(uint32_t id, const Vec3& pos, const AABB& bounds,
                                 uint32_t visual_ref, RegionID region) const;
    ResourceRecord normalizeResource(ResourceID id, const std::string& name,
                                     const std::string& path, uint64_t hash,
                                     const AABB& bounds) const;
    CollisionRecord normalizeCollision(CollisionID id, ShapeType type,
                                       const AABB& bounds) const;
    ScanMaterialRecord normalizeMaterial(MaterialID id, const RGBA& base_color) const;
    TextureRecord normalizeTexture(TextureID id, const std::string& path,
                                    int w, int h) const;
};

} // namespace mgd
