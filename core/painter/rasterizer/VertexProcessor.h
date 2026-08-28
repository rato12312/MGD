#pragma once

#include "../RenderVertex.h"
#include "../../common/Mat4.h"
#include <vector>

namespace mgd {

class VertexProcessor {
public:
    struct ClipResult {
        std::vector<RenderVertex> vertices;
    };

    static RenderVertex transformVertex(const RenderVertex& v, const Mat4& mvp,
                                        int screen_w, int screen_h);
    static ClipResult clipTriangle(const RenderVertex& v0, const RenderVertex& v1, const RenderVertex& v2);
    static float triangleArea2D(const Vec2& a, const Vec2& b, const Vec2& c);
    static Vec3 barycentricCoords(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c);
};

} // namespace mgd
