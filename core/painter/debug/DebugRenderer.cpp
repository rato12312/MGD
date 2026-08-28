#include "DebugRenderer.h"
#include <algorithm>
#include <cmath>

namespace mgd {

static Vec3 projectToScreen(const Vec3& pos, const Mat4& mvp, int sw, int sh) {
    Vec4 clip = mvp.transformPoint(Vec4(pos, 1.0f));
    if (std::abs(clip.w) < 1e-8f) return {-1000.0f, -1000.0f, -1000.0f};
    float invW = 1.0f / clip.w;
    float ndcX = clip.x * invW;
    float ndcY = clip.y * invW;
    float sx = (ndcX + 1.0f) * 0.5f * sw;
    float sy = (1.0f - ndcY) * 0.5f * sh;
    float sz = clip.z * invW; // NDC z in [0, 1] (D3D)
    return {sx, sy, sz};
}

void DebugRenderer::renderWireframe(Framebuffer& fb, const std::vector<RenderVertex>& verts,
                                    const std::vector<uint32_t>& indices, const Mat4& mvp,
                                    int sw, int sh) {
    RGBA wireColor = {0, 255, 0, 255};

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vec3 s0 = projectToScreen(verts[indices[i]].position, mvp, sw, sh);
        Vec3 s1 = projectToScreen(verts[indices[i + 1]].position, mvp, sw, sh);
        Vec3 s2 = projectToScreen(verts[indices[i + 2]].position, mvp, sw, sh);

        fb.drawLine(static_cast<int>(s0.x), static_cast<int>(s0.y),
                    static_cast<int>(s1.x), static_cast<int>(s1.y), wireColor);
        fb.drawLine(static_cast<int>(s1.x), static_cast<int>(s1.y),
                    static_cast<int>(s2.x), static_cast<int>(s2.y), wireColor);
        fb.drawLine(static_cast<int>(s2.x), static_cast<int>(s2.y),
                    static_cast<int>(s0.x), static_cast<int>(s0.y), wireColor);
    }
}

void DebugRenderer::renderDepth(Framebuffer& fb, const DepthBuffer& db) {
    int w = db.width();
    int h = db.height();
    if (w == 0 || h == 0) return;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float depth = db.get(x, y);
            uint8_t c = static_cast<uint8_t>(depth * 255.0f);
            fb.setPixel(x, y, {c, c, c, 255});
        }
    }
}

void DebugRenderer::renderBounds(Framebuffer& fb, const AABB& box, const Mat4& mvp,
                                 int sw, int sh, const RGBA& color) {
    Vec3 corners[8] = {
        {box.min.x, box.min.y, box.min.z},
        {box.max.x, box.min.y, box.min.z},
        {box.max.x, box.max.y, box.min.z},
        {box.min.x, box.max.y, box.min.z},
        {box.min.x, box.min.y, box.max.z},
        {box.max.x, box.min.y, box.max.z},
        {box.max.x, box.max.y, box.max.z},
        {box.min.x, box.max.y, box.max.z}
    };

    Vec3 screen[8];
    for (int i = 0; i < 8; ++i) {
        screen[i] = projectToScreen(corners[i], mvp, sw, sh);
    }

    auto drawEdge = [&](int a, int b) {
        fb.drawLine(static_cast<int>(screen[a].x), static_cast<int>(screen[a].y),
                    static_cast<int>(screen[b].x), static_cast<int>(screen[b].y), color);
    };

    drawEdge(0, 1); drawEdge(1, 2); drawEdge(2, 3); drawEdge(3, 0);
    drawEdge(4, 5); drawEdge(5, 6); drawEdge(6, 7); drawEdge(7, 4);
    drawEdge(0, 4); drawEdge(1, 5); drawEdge(2, 6); drawEdge(3, 7);
}

} // namespace mgd
