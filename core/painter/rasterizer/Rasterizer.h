#pragma once

#include "../RenderVertex.h"
#include "../framebuffer/Framebuffer.h"
#include "../framebuffer/DepthBuffer.h"
#include "../texture/TextureData.h"
#include "../material/MaterialRecord.h"

namespace mgd {

class Rasterizer {
public:
    static uint32_t rasterizeTriangle(
        const RenderVertex& v0, const RenderVertex& v1, const RenderVertex& v2,
        Framebuffer& fb,
        DepthBuffer& db,
        const TextureData* tex,
        const MaterialRecord& mat,
        bool backface_culling,
        bool depth_test
    );
};

} // namespace mgd
