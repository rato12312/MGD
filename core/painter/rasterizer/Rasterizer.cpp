#include "Rasterizer.h"
#include "VertexProcessor.h"
#include "../texture/TextureSampler.h"
#include <algorithm>
#include <cmath>

namespace mgd {

void Rasterizer::rasterizeTriangle(
    const RenderVertex& v0, const RenderVertex& v1, const RenderVertex& v2,
    Framebuffer& fb,
    DepthBuffer& db,
    const TextureData* tex,
    const MaterialRecord& mat,
    bool backface_culling,
    bool depth_test)
{
    Vec2 p0 = {v0.position.x, v0.position.y};
    Vec2 p1 = {v1.position.x, v1.position.y};
    Vec2 p2 = {v2.position.x, v2.position.y};

    float area = VertexProcessor::triangleArea2D(p0, p1, p2);

    if (backface_culling && area >= 0.0f) {
        return;
    }

    float minX = std::min({p0.x, p1.x, p2.x});
    float minY = std::min({p0.y, p1.y, p2.y});
    float maxX = std::max({p0.x, p1.x, p2.x});
    float maxY = std::max({p0.y, p1.y, p2.y});

    int startX = std::max(0, static_cast<int>(std::floor(minX)));
    int startY = std::max(0, static_cast<int>(std::floor(minY)));
    int endX = std::min(fb.width() - 1, static_cast<int>(std::ceil(maxX)));
    int endY = std::min(fb.height() - 1, static_cast<int>(std::ceil(maxY)));

    float invArea = 1.0f / area;

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            Vec2 pixel = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};

            float e0 = (p1.x - p0.x) * (pixel.y - p0.y) - (p1.y - p0.y) * (pixel.x - p0.x);
            float e1 = (p2.x - p1.x) * (pixel.y - p1.y) - (p2.y - p1.y) * (pixel.x - p1.x);
            float e2 = (p0.x - p2.x) * (pixel.y - p2.y) - (p0.y - p2.y) * (pixel.x - p2.x);

            bool sameSign = false;
            if (area < 0.0f) {
                sameSign = (e0 < 0.0f) && (e1 < 0.0f) && (e2 < 0.0f);
            } else {
                sameSign = (e0 > 0.0f) && (e1 > 0.0f) && (e2 > 0.0f);
            }

            if (!sameSign) continue;

            float lambda0 = e0 * invArea;
            float lambda1 = e1 * invArea;
            float lambda2 = e2 * invArea;

            // Depth: linear in screen space (correct for z-buffer)
            float z = lambda0 * v0.position.z + lambda1 * v1.position.z + lambda2 * v2.position.z;

            if (depth_test && !db.inBounds(x, y)) continue;
            if (depth_test && !db.test(x, y, z)) continue;

            // Perspective-correct interpolation
            float w0 = lambda0 * v0.rhw;
            float w1 = lambda1 * v1.rhw;
            float w2 = lambda2 * v2.rhw;
            float wSum = w0 + w1 + w2;
            float invWSum = (std::abs(wSum) > 1e-10f) ? 1.0f / wSum : 0.0f;

            float pcU = (lambda0 * v0.uv.x * v0.rhw + lambda1 * v1.uv.x * v1.rhw + lambda2 * v2.uv.x * v2.rhw) * invWSum;
            float pcV = (lambda0 * v0.uv.y * v0.rhw + lambda1 * v1.uv.y * v1.rhw + lambda2 * v2.uv.y * v2.rhw) * invWSum;

            float nx = (lambda0 * v0.normal.x * v0.rhw + lambda1 * v1.normal.x * v1.rhw + lambda2 * v2.normal.x * v2.rhw) * invWSum;
            float ny = (lambda0 * v0.normal.y * v0.rhw + lambda1 * v1.normal.y * v1.rhw + lambda2 * v2.normal.y * v2.rhw) * invWSum;
            float nz = (lambda0 * v0.normal.z * v0.rhw + lambda1 * v1.normal.z * v1.rhw + lambda2 * v2.normal.z * v2.rhw) * invWSum;

            RGBA finalColor;
            if (tex) {
                finalColor = TextureSampler::sample(*tex, pcU, pcV);
            } else {
                float r = lambda0 * v0.color.r + lambda1 * v1.color.r + lambda2 * v2.color.r;
                float g = lambda0 * v0.color.g + lambda1 * v1.color.g + lambda2 * v2.color.g;
                float b = lambda0 * v0.color.b + lambda1 * v1.color.b + lambda2 * v2.color.b;
                finalColor.r = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
                finalColor.g = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
                finalColor.b = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
                finalColor.a = 255;
            }

            if (finalColor.a < 128) continue;

            if (depth_test) {
                db.write(x, y, z);
            }
            fb.setPixel(x, y, finalColor);
        }
    }
}

} // namespace mgd
