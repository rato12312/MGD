#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/painter/rasterizer/Rasterizer.h"
#include "core/painter/rasterizer/VertexProcessor.h"
#include "core/painter/framebuffer/Framebuffer.h"
#include "core/painter/framebuffer/DepthBuffer.h"
#include "core/painter/material/MaterialRecord.h"
#include "core/common/Vec2.h"

using namespace mgd;

bool run_rasterizer_tests() {
    Framebuffer fb(320, 240);
    DepthBuffer db(320, 240);
    MaterialRecord mat;
    mat.id = 1;
    mat.base_color = RGBA(255, 255, 255, 255);

    fb.clear(RGBA(0, 0, 0, 255));
    db.clear(1.0f);

    RenderVertex v0;
    v0.position = Vec3(50.0f, 50.0f, 0.2f);
    v0.normal = Vec3(0, 0, 1);
    v0.uv = Vec2(0, 0);
    v0.color = RGBA(255, 0, 0, 255);
    v0.rhw = 1.0f;

    RenderVertex v1;
    v1.position = Vec3(200.0f, 50.0f, 0.2f);
    v1.normal = Vec3(0, 0, 1);
    v1.uv = Vec2(1, 0);
    v1.color = RGBA(0, 255, 0, 255);
    v1.rhw = 1.0f;

    RenderVertex v2;
    v2.position = Vec3(100.0f, 200.0f, 0.2f);
    v2.normal = Vec3(0, 0, 1);
    v2.uv = Vec2(0.5f, 1);
    v2.color = RGBA(0, 0, 255, 255);
    v2.rhw = 1.0f;

    Rasterizer::rasterizeTriangle(v0, v1, v2, fb, db, nullptr, mat, false, true);

    RGBA pixelCenter = fb.getPixel(110, 100);
    ASSERT_MSG(pixelCenter.r != 0 || pixelCenter.g != 0 || pixelCenter.b != 0,
               "center of triangle should have some color");

    RGBA pixelOutside = fb.getPixel(10, 10);
    ASSERT_EQ(pixelOutside.r, static_cast<uint8_t>(0));
    ASSERT_EQ(pixelOutside.g, static_cast<uint8_t>(0));
    ASSERT_EQ(pixelOutside.b, static_cast<uint8_t>(0));

    RGBA pixelTopLeft = fb.getPixel(55, 55);
    ASSERT_MSG(pixelTopLeft.r > 100, "near v0 should have red component");

    RGBA pixelTopRight = fb.getPixel(190, 55);
    ASSERT_MSG(pixelTopRight.g > 100, "near v1 should have green component");

    Framebuffer fb2(320, 240);
    DepthBuffer db2(320, 240);
    fb2.clear(RGBA(0, 0, 0, 255));
    db2.clear(1.0f);

    RenderVertex nearV0;
    nearV0.position = Vec3(80.0f, 80.0f, 0.1f);
    nearV0.normal = Vec3(0, 0, 1);
    nearV0.uv = Vec2(0, 0);
    nearV0.color = RGBA(255, 0, 0, 255);
    nearV0.rhw = 1.0f;

    RenderVertex nearV1;
    nearV1.position = Vec3(240.0f, 80.0f, 0.1f);
    nearV1.normal = Vec3(0, 0, 1);
    nearV1.uv = Vec2(1, 0);
    nearV1.color = RGBA(255, 0, 0, 255);
    nearV1.rhw = 1.0f;

    RenderVertex nearV2;
    nearV2.position = Vec3(160.0f, 200.0f, 0.1f);
    nearV2.normal = Vec3(0, 0, 1);
    nearV2.uv = Vec2(0.5f, 1);
    nearV2.color = RGBA(255, 0, 0, 255);
    nearV2.rhw = 1.0f;

    Rasterizer::rasterizeTriangle(nearV0, nearV1, nearV2, fb2, db2, nullptr, mat, false, true);

    RenderVertex farV0;
    farV0.position = Vec3(100.0f, 100.0f, 0.5f);
    farV0.normal = Vec3(0, 0, 1);
    farV0.uv = Vec2(0, 0);
    farV0.color = RGBA(0, 255, 0, 255);
    farV0.rhw = 1.0f;

    RenderVertex farV1;
    farV1.position = Vec3(220.0f, 100.0f, 0.5f);
    farV1.normal = Vec3(0, 0, 1);
    farV1.uv = Vec2(1, 0);
    farV1.color = RGBA(0, 255, 0, 255);
    farV1.rhw = 1.0f;

    RenderVertex farV2;
    farV2.position = Vec3(160.0f, 180.0f, 0.5f);
    farV2.normal = Vec3(0, 0, 1);
    farV2.uv = Vec2(0.5f, 1);
    farV2.color = RGBA(0, 255, 0, 255);
    farV2.rhw = 1.0f;

    Rasterizer::rasterizeTriangle(farV0, farV1, farV2, fb2, db2, nullptr, mat, false, true);

    RGBA depthTestPixel = fb2.getPixel(160, 120);
    ASSERT_MSG(depthTestPixel.r > 100, "closer triangle (red) should win at overlapping pixel");

    Framebuffer fbNoDepth(320, 240);
    DepthBuffer dbNoDepth(320, 240);
    fbNoDepth.clear(RGBA(0, 0, 0, 255));
    dbNoDepth.clear(1.0f);

    Rasterizer::rasterizeTriangle(farV0, farV1, farV2, fbNoDepth, dbNoDepth, nullptr, mat, false, false);
    RGBA noDepthPixel = fbNoDepth.getPixel(160, 120);
    ASSERT_MSG(noDepthPixel.g > 100, "without depth test, far triangle should be visible");

    Framebuffer fbCull(320, 240);
    DepthBuffer dbCull(320, 240);
    fbCull.clear(RGBA(0, 0, 0, 255));
    dbCull.clear(1.0f);

    Rasterizer::rasterizeTriangle(v0, v1, v2, fbCull, dbCull, nullptr, mat, true, false);
    RGBA culledPixel = fbCull.getPixel(110, 100);
    ASSERT_MSG(culledPixel.r == 0 && culledPixel.g == 0 && culledPixel.b == 0,
               "backface culled triangle should not be rasterized");

    float area = VertexProcessor::triangleArea2D(Vec2(0, 0), Vec2(1, 0), Vec2(0, 1));
    ASSERT_NEAR(area, -1.0f, 0.01f);

    float area2 = VertexProcessor::triangleArea2D(Vec2(0, 0), Vec2(0, 1), Vec2(1, 0));
    ASSERT_NEAR(area2, 1.0f, 0.01f);

    Vec3 bary = VertexProcessor::barycentricCoords(
        Vec2(0.25f, 0.25f), Vec2(0, 0), Vec2(1, 0), Vec2(0, 1));
    ASSERT_NEAR(bary.x + bary.y + bary.z, 1.0f, 0.01f);
    ASSERT_MSG(bary.x > 0 && bary.y > 0 && bary.z > 0, "center point should have all positive barycentrics");

    RenderVertex xformV;
    xformV.position = Vec3(0.5f, 0.5f, 0.5f);
    xformV.normal = Vec3(0, 0, 1);
    xformV.uv = Vec2(0, 0);
    xformV.color = RGBA(255, 255, 255, 255);
    xformV.rhw = 1.0f;

    Mat4 identity = Mat4::identity();
    RenderVertex xformed = VertexProcessor::transformVertex(xformV, identity, 800, 600);
    ASSERT_MSG(xformed.position.x > 0 && xformed.position.x < 800,
               "transformed x should be within screen");
    ASSERT_MSG(xformed.position.y > 0 && xformed.position.y < 600,
               "transformed y should be within screen");

    Framebuffer fbEdge(320, 240);
    DepthBuffer dbEdge(320, 240);
    fbEdge.clear(RGBA(0, 0, 0, 255));
    dbEdge.clear(1.0f);

    RenderVertex edgeV0;
    edgeV0.position = Vec3(0.0f, 0.0f, 0.5f);
    edgeV0.normal = Vec3(0, 0, 1);
    edgeV0.uv = Vec2(0, 0);
    edgeV0.color = RGBA(100, 100, 100, 255);
    edgeV0.rhw = 1.0f;

    RenderVertex edgeV1;
    edgeV1.position = Vec3(319.0f, 0.0f, 0.5f);
    edgeV1.normal = Vec3(0, 0, 1);
    edgeV1.uv = Vec2(1, 0);
    edgeV1.color = RGBA(100, 100, 100, 255);
    edgeV1.rhw = 1.0f;

    RenderVertex edgeV2;
    edgeV2.position = Vec3(160.0f, 239.0f, 0.5f);
    edgeV2.normal = Vec3(0, 0, 1);
    edgeV2.uv = Vec2(0.5f, 1);
    edgeV2.color = RGBA(100, 100, 100, 255);
    edgeV2.rhw = 1.0f;

    Rasterizer::rasterizeTriangle(edgeV0, edgeV1, edgeV2, fbEdge, dbEdge, nullptr, mat, false, true);
    RGBA edgePixel = fbEdge.getPixel(160, 120);
    ASSERT_MSG(edgePixel.r > 0, "large triangle covering screen should have pixels");

    std::cout << "  Rasterizer tests passed!" << std::endl;
    return true;
}
