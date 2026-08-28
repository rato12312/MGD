#include "VertexProcessor.h"
#include <algorithm>
#include <cmath>

namespace mgd {

RenderVertex VertexProcessor::transformVertex(const RenderVertex& v, const Mat4& mvp,
                                              int screen_w, int screen_h) {
    RenderVertex out = v;

    Vec4 clip = mvp.transformPoint(Vec4(v.position, 1.0f));

    float invW = (std::abs(clip.w) > 1e-8f) ? 1.0f / clip.w : 0.0f;

    out.position.x = (clip.x * invW * 0.5f + 0.5f) * screen_w;
    out.position.y = (1.0f - (clip.y * invW * 0.5f + 0.5f)) * screen_h;
    out.position.z = (clip.z * invW * 0.5f + 0.5f);

    out.rhw = invW;

    out.normal = mvp.transformDirection(v.normal).normalized();

    return out;
}

VertexProcessor::ClipResult VertexProcessor::clipTriangle(
    const RenderVertex& v0, const RenderVertex& v1, const RenderVertex& v2) {

    ClipResult result;

    std::vector<RenderVertex> input = {v0, v1, v2};

    auto clipAxis = [&](bool keepPositive, int axis, bool isW) {
        if (input.empty()) return;

        std::vector<RenderVertex> output;
        for (size_t i = 0; i < input.size(); ++i) {
            const RenderVertex& curr = input[i];
            const RenderVertex& next = input[(i + 1) % input.size()];

            float currVal = isW ? curr.rhw : (&curr.position.x)[axis];
            float nextVal = isW ? next.rhw : (&next.position.x)[axis];

            bool currInside = keepPositive ? (currVal >= 0.0f) : (currVal <= 0.0f);
            bool nextInside = keepPositive ? (nextVal >= 0.0f) : (nextVal <= 0.0f);

            if (currInside) {
                output.push_back(curr);
                if (!nextInside) {
                    float t = currVal / (currVal - nextVal);
                    RenderVertex interp;
                    interp.position.x = curr.position.x + (next.position.x - curr.position.x) * t;
                    interp.position.y = curr.position.y + (next.position.y - curr.position.y) * t;
                    interp.position.z = curr.position.z + (next.position.z - curr.position.z) * t;
                    interp.normal = Vec3(
                        curr.normal.x + (next.normal.x - curr.normal.x) * t,
                        curr.normal.y + (next.normal.y - curr.normal.y) * t,
                        curr.normal.z + (next.normal.z - curr.normal.z) * t
                    );
                    interp.uv = Vec2(
                        curr.uv.x + (next.uv.x - curr.uv.x) * t,
                        curr.uv.y + (next.uv.y - curr.uv.y) * t
                    );
                    interp.color.r = static_cast<uint8_t>(curr.color.r + (next.color.r - curr.color.r) * t);
                    interp.color.g = static_cast<uint8_t>(curr.color.g + (next.color.g - curr.color.g) * t);
                    interp.color.b = static_cast<uint8_t>(curr.color.b + (next.color.b - curr.color.b) * t);
                    interp.color.a = static_cast<uint8_t>(curr.color.a + (next.color.a - curr.color.a) * t);
                    interp.rhw = curr.rhw + (next.rhw - curr.rhw) * t;
                    output.push_back(interp);
                }
            } else if (nextInside) {
                float t = currVal / (currVal - nextVal);
                RenderVertex interp;
                interp.position.x = curr.position.x + (next.position.x - curr.position.x) * t;
                interp.position.y = curr.position.y + (next.position.y - curr.position.y) * t;
                interp.position.z = curr.position.z + (next.position.z - curr.position.z) * t;
                interp.normal = Vec3(
                    curr.normal.x + (next.normal.x - curr.normal.x) * t,
                    curr.normal.y + (next.normal.y - curr.normal.y) * t,
                    curr.normal.z + (next.normal.z - curr.normal.z) * t
                );
                interp.uv = Vec2(
                    curr.uv.x + (next.uv.x - curr.uv.x) * t,
                    curr.uv.y + (next.uv.y - curr.uv.y) * t
                );
                interp.color.r = static_cast<uint8_t>(curr.color.r + (next.color.r - curr.color.r) * t);
                interp.color.g = static_cast<uint8_t>(curr.color.g + (next.color.g - curr.color.g) * t);
                interp.color.b = static_cast<uint8_t>(curr.color.b + (next.color.b - curr.color.b) * t);
                interp.color.a = static_cast<uint8_t>(curr.color.a + (next.color.a - curr.color.a) * t);
                interp.rhw = curr.rhw + (next.rhw - curr.rhw) * t;
                output.push_back(interp);
            }
        }
        input = output;
    };

    clipAxis(true, 0, false);   // +X (right)
    clipAxis(false, 0, false);  // -X (left)
    clipAxis(true, 1, false);   // +Y (top)
    clipAxis(false, 1, false);  // -Y (bottom)
    clipAxis(false, 2, false);  // +Z (near, NDC z >= 0)
    clipAxis(true, 2, false);   // -Z (far, NDC z <= 1)

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

    float d00 = v0.x * v0.x; // not needed, using cross-product form
    float d01 = v0.dot(v1);
    float d11 = v1.dot(v1);
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
