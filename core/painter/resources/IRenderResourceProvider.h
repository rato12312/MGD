#pragma once

#include "../RenderVertex.h"
#include "../texture/TextureData.h"
#include "../material/MaterialRecord.h"
#include "../../common/Types.h"
#include "../../common/AABB.h"
#include <vector>

namespace mgd {

struct MeshData {
    std::vector<RenderVertex> vertices;
    std::vector<uint32_t> indices;
    AABB bounds;
};

class IRenderResourceProvider {
public:
    virtual ~IRenderResourceProvider() = default;
    virtual const MeshData* getMesh(MeshID id) = 0;
    virtual const TextureData* getTexture(TextureID id) = 0;
    virtual const MaterialRecord* getMaterial(MaterialID id) = 0;
};

} // namespace mgd
