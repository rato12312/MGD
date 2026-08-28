#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/painter/framebuffer/Framebuffer.h"
#include "core/painter/framebuffer/DepthBuffer.h"

using namespace mgd;

bool run_framebuffer_tests() {
    Framebuffer fb;
    ASSERT_EQ(fb.width(), 0);
    ASSERT_EQ(fb.height(), 0);

    fb.resize(320, 240);
    ASSERT_EQ(fb.width(), 320);
    ASSERT_EQ(fb.height(), 240);
    ASSERT_EQ(fb.stride(), 320 * 4);

    fb.clear(RGBA(255, 0, 0, 255));
    RGBA pixel = fb.getPixel(0, 0);
    ASSERT_EQ(pixel.r, static_cast<uint8_t>(255));
    ASSERT_EQ(pixel.g, static_cast<uint8_t>(0));
    ASSERT_EQ(pixel.b, static_cast<uint8_t>(0));
    ASSERT_EQ(pixel.a, static_cast<uint8_t>(255));

    RGBA corner = fb.getPixel(319, 239);
    ASSERT_EQ(corner.r, static_cast<uint8_t>(255));

    fb.setPixel(160, 120, RGBA(0, 255, 0, 200));
    RGBA mid = fb.getPixel(160, 120);
    ASSERT_EQ(mid.r, static_cast<uint8_t>(0));
    ASSERT_EQ(mid.g, static_cast<uint8_t>(255));
    ASSERT_EQ(mid.b, static_cast<uint8_t>(0));
    ASSERT_EQ(mid.a, static_cast<uint8_t>(200));

    fb.setPixel(50, 50, RGBA(10, 20, 30, 40));
    RGBA set = fb.getPixel(50, 50);
    ASSERT_EQ(set.r, static_cast<uint8_t>(10));
    ASSERT_EQ(set.g, static_cast<uint8_t>(20));
    ASSERT_EQ(set.b, static_cast<uint8_t>(30));
    ASSERT_EQ(set.a, static_cast<uint8_t>(40));

    RGBA notPixel = fb.getPixel(0, 0);
    ASSERT_EQ(notPixel.r, static_cast<uint8_t>(255));

    fb.clear(RGBA(0, 0, 0, 255));
    RGBA cleared = fb.getPixel(160, 120);
    ASSERT_EQ(cleared.r, static_cast<uint8_t>(0));
    ASSERT_EQ(cleared.g, static_cast<uint8_t>(0));
    ASSERT_EQ(cleared.b, static_cast<uint8_t>(0));

    ASSERT_MSG(fb.inBounds(0, 0), "(0,0) should be in bounds");
    ASSERT_MSG(fb.inBounds(319, 239), "(319,239) should be in bounds");
    ASSERT_MSG(!fb.inBounds(-1, 0), "(-1,0) should not be in bounds");
    ASSERT_MSG(!fb.inBounds(0, -1), "(0,-1) should not be in bounds");
    ASSERT_MSG(!fb.inBounds(320, 0), "(320,0) should not be in bounds");
    ASSERT_MSG(!fb.inBounds(0, 240), "(0,240) should not be in bounds");

    fb.drawLine(0, 0, 10, 0, RGBA(0, 0, 255, 255));
    ASSERT_EQ(fb.getPixel(0, 0).b, static_cast<uint8_t>(255));
    ASSERT_EQ(fb.getPixel(5, 0).b, static_cast<uint8_t>(255));
    ASSERT_EQ(fb.getPixel(10, 0).b, static_cast<uint8_t>(255));

    const uint8_t* rawData = fb.data();
    ASSERT_MSG(rawData != nullptr, "data pointer should not be null");

    uint8_t* mutableData = fb.data();
    ASSERT_MSG(mutableData != nullptr, "mutable data pointer should not be null");

    Framebuffer fb2(100, 100);
    ASSERT_EQ(fb2.width(), 100);
    ASSERT_EQ(fb2.height(), 100);

    fb2.clear(RGBA(128, 128, 128, 255));
    RGBA fb2pixel = fb2.getPixel(50, 50);
    ASSERT_EQ(fb2pixel.r, static_cast<uint8_t>(128));
    ASSERT_EQ(fb2pixel.g, static_cast<uint8_t>(128));

    fb2.resize(200, 200);
    ASSERT_EQ(fb2.width(), 200);
    ASSERT_EQ(fb2.height(), 200);

    DepthBuffer db;
    ASSERT_EQ(db.width(), 0);
    ASSERT_EQ(db.height(), 0);

    db.resize(320, 240);
    ASSERT_EQ(db.width(), 320);
    ASSERT_EQ(db.height(), 240);

    db.clear(1.0f);
    ASSERT_MSG(db.test(10, 10, 0.5f), "depth 0.5 should pass against clear value 1.0");
    ASSERT_MSG(!db.test(10, 10, 1.5f), "depth 1.5 should fail against clear value 1.0");
    ASSERT_MSG(!db.test(10, 10, 1.0f), "depth 1.0 should fail (not <) against clear value 1.0");

    db.write(10, 10, 0.5f);
    float written = db.get(10, 10);
    ASSERT_NEAR(written, 0.5f, 0.001f);

    ASSERT_MSG(db.test(10, 10, 0.3f), "depth 0.3 should pass against 0.5");
    ASSERT_MSG(!db.test(10, 10, 0.6f), "depth 0.6 should fail against 0.5");

    ASSERT_MSG(db.inBounds(0, 0), "(0,0) should be in bounds");
    ASSERT_MSG(db.inBounds(319, 239), "(319,239) should be in bounds");
    ASSERT_MSG(!db.inBounds(-1, 0), "(-1,0) should not be in bounds");
    ASSERT_MSG(!db.inBounds(320, 240), "(320,240) should not be in bounds");

    db.clear(0.0f);
    ASSERT_MSG(!db.test(0, 0, 0.0f), "depth 0.0 should fail against clear value 0.0");
    ASSERT_MSG(db.test(0, 0, -0.1f), "negative depth should pass against 0.0");

    std::cout << "  Framebuffer tests passed!" << std::endl;
    return true;
}
