#pragma once

#include "../framebuffer/Framebuffer.h"
#include "../framebuffer/DepthBuffer.h"
#include "../RenderVertex.h"
#include "../../common/Mat4.h"
#include "../../common/AABB.h"
#include "../../common/RGBA.h"
#include <vector>

namespace mgd {

class DebugRenderer {
public:
    static void renderWireframe(Framebuffer& fb, const std::vector<RenderVertex>& verts,
                                const std::vector<uint32_t>& indices, const Mat4& mvp,
                                int sw, int sh);
    static void renderDepth(Framebuffer& fb, const DepthBuffer& db);
    static void renderBounds(Framebuffer& fb, const AABB& box, const Mat4& mvp,
                             int sw, int sh, const RGBA& color);
};

} // namespace mgd
