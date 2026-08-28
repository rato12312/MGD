#include "VertexProcessor.h"
#include <algorithm>
#include <cmath>

namespace mgd {

VertexProcessor::ClipVertex VertexProcessor::toClipSpace(const RenderVertex& v, const Mat4& mvp) {
    ClipVertex cv;
    cv.position = mvp.transformPoint(Vec4(v.position, 1.0f));
    cv.normal = mvp.transformDirection(v.normal).normalized();
    cv.uv = v.uv;
    cv.color = v.color;
    return cv;
}

RenderVertex VertexProcessor::toScreen(const ClipVertex& v, int screen_w, int screen_h) {
    RenderVertex out;

    float w = v.position.w;
    float invW = (std::abs(w) > 1e-8f) ? 1.0f / w : 0.0f;

    float ndcX = v.position.x * invW;
    float ndcY = v.position.y * invW;
    float ndcZ = v.position.z * invW;

    out.position.x = (ndcX * 0.5f + 0.5f) * screen_w;
    out.position.y = (1.0f - (ndcY * 0.5f + 0.5f)) * screen_h;
    out.position.z = ndcZ; // NDC z in [0, 1] (D3D convention)
    out.rhw = invW;

    out.normal = v.normal;
    out.uv = v.uv;
    out.color = v.color;

    return out;
}

RenderVertex VertexProcessor::transformVertex(const RenderVertex& v, const Mat4& mvp,
                                              int screen_w, int screen_h) {
    return toScreen(toClipSpace(v, mvp), screen_w, screen_h);
}

static VertexProcessor::ClipVertex lerpClipVertex(const VertexProcessor::ClipVertex& a,
                                                  const VertexProcessor::ClipVertex& b, float t) {
    VertexProcessor::ClipVertex out;
    out.position = Vec4(
        a.position.x + (b.position.x - a.position.x) * t,
        a.position.y + (b.position.y - a.position.y) * t,
        a.position.z + (b.position.z - a.position.z) * t,
        a.position.w + (b.position.w - a.position.w) * t
    );
    out.normal = Vec3(
        a.normal.x + (b.normal.x - a.normal.x) * t,
        a.normal.y + (b.normal.y - a.normal.y) * t,
        a.normal.z + (b.normal.z - a.normal.z) * t
    );
    out.uv = Vec2(
        a.uv.x + (b.uv.x - a.uv.x) * t,
        a.uv.y + (b.uv.y - a.uv.y) * t
    );
    out.color.r = static_cast<uint8_t>(a.color.r + (b.color.r - a.color.r) * t);
    out.color.g = static_cast<uint8_t>(a.color.g + (b.color.g - a.color.g) * t);
    out.color.b = static_cast<uint8_t>(a.color.b + (b.color.b - a.color.b) * t);
    out.color.a = static_cast<uint8_t>(a.color.a + (b.color.a - a.color.a) * t);
    return out;
}

VertexProcessor::ClipResult VertexProcessor::clipTriangle(
    const ClipVertex& v0, const ClipVertex& v1, const ClipVertex& v2) {

    ClipResult result;
    std::vector<ClipVertex> input = {v0, v1, v2};

    // Sutherland-Hodgman in clip space (D3D convention: near plane at z=0,
    // far plane at z=w). Each distance is >= 0 when the vertex is inside.
    auto planeDistance = [](const ClipVertex& v, int plane) -> float {
        switch (plane) {
            case 0: return v.position.x + v.position.w; // left:   x >= -w
            case 1: return v.position.w - v.position.x; // right:  x <=  w
            case 2: return v.position.y + v.position.w; // bottom: y >= -w
            case 3: return v.position.w - v.position.y; // top:    y <=  w
            case 4: return v.position.z;                // near:   z >=  0
            default: return v.position.w - v.position.z; // far:    z <=  w
        }
    };

    for (int plane = 0; plane < 6; ++plane) {
        if (input.empty()) break;

        std::vector<ClipVertex> output;
        for (size_t i = 0; i < input.size(); ++i) {
            const ClipVertex& curr = input[i];
            const ClipVertex& next = input[(i + 1) % input.size()];

            float currD = planeDistance(curr, plane);
            float nextD = planeDistance(next, plane);

            bool currInside = currD >= 0.0f;
            bool nextInside = nextD >= 0.0f;

            if (currInside) {
                output.push_back(curr);
            }
            if (currInside != nextInside) {
                float t = currD / (currD - nextD);
                output.push_back(lerpClipVertex(curr, next, t));
            }
        }
        input = std::move(output);
    }

    result.vertices = std::move(input);
    return result;
}

float VertexProcessor::triangleArea2D(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

Vec3 VertexProcessor::barycentricCoords(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) {
    Vec2 v0 = b - a;
    Vec2 v1 = c - a;
    Vec2 v2 = p - a;

    float d00 = v0.lengthSq();
    float d01 = v0.dot(v1);
    float d11 = v1.lengthSq();
    float d20 = v2.dot(v0);
    float d21 = v2.dot(v1);

    float denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 1e-10f) return {-1.0f, -1.0f, -1.0f};

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    return {u, v, w};
}

} // namespace mgd