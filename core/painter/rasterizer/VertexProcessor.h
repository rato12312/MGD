#pragma once

#include "../RenderVertex.h"
#include "../../common/Mat4.h"
#include "../../common/Vec4.h"
#include <vector>

namespace mgd {

class VertexProcessor {
public:
    // Vertex in clip space (after model-view-projection, before perspective divide).
    struct ClipVertex {
        Vec4 position;   // (x, y, z, w) in clip space
        Vec3 normal;
        Vec2 uv;
        RGBA color;
    };

    struct ClipResult {
        std::vector<ClipVertex> vertices;
    };

    static ClipVertex toClipSpace(const RenderVertex& v, const Mat4& mvp);
    static RenderVertex toScreen(const ClipVertex& v, int screen_w, int screen_h);
    static ClipResult clipTriangle(const ClipVertex& v0, const ClipVertex& v1, const ClipVertex& v2);
    static RenderVertex transformVertex(const RenderVertex& v, const Mat4& mvp,
                                        int screen_w, int screen_h);
    static float triangleArea2D(const Vec2& a, const Vec2& b, const Vec2& c);
    static Vec3 barycentricCoords(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c);
};

} // namespace mgd