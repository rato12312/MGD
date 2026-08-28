#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/painter/Painter.h"
#include "core/painter/RenderFrameInput.h"
#include "core/painter/resources/MockResourceProvider.h"
#include "core/visibility/VisibleSet.h"
#include "core/camera/Camera.h"

using namespace mgd;

bool run_painter_tests() {
    Painter painter;
    painter.initialize(800, 600);

    RenderSettings& settings = painter.getSettings();
    ASSERT_EQ(settings.width, 800);
    ASSERT_EQ(settings.height, 600);

    painter.resize(1024, 768);
    RenderSettings& settings2 = painter.getSettings();
    ASSERT_EQ(settings2.width, 1024);
    ASSERT_EQ(settings2.height, 768);

    Framebuffer& fb = painter.getFramebuffer();
    ASSERT_EQ(fb.width(), 1024);
    ASSERT_EQ(fb.height(), 768);

    DepthBuffer& db = painter.getDepthBuffer();
    ASSERT_EQ(db.width(), 1024);
    ASSERT_EQ(db.height(), 768);

    RenderSettings defaultSettings;
    ASSERT_EQ(defaultSettings.width, 800);
    ASSERT_EQ(defaultSettings.height, 600);
    ASSERT_EQ(defaultSettings.target_fps, 30);
    ASSERT_MSG(defaultSettings.backface_culling == true, "default backface culling should be on");
    ASSERT_MSG(defaultSettings.depth_test == true, "default depth test should be on");
    ASSERT_MSG(defaultSettings.lighting_enabled == true, "default lighting should be on");
    ASSERT_MSG(defaultSettings.shadows_enabled == false, "default shadows should be off");
    ASSERT_EQ(defaultSettings.debug_mode, DebugMode::NORMAL);
    ASSERT_NEAR(defaultSettings.near_plane, 0.1f, 0.001f);
    ASSERT_NEAR(defaultSettings.far_plane, 1000.0f, 0.001f);

    painter.initialize(800, 600);

    Camera camera;
    camera.setPosition(Vec3(0, 0, 5));
    camera.setOrientation(Vec3(0, 0, 0));

    MockResourceProvider resources;
    VisibleSet visible;

    RenderFrameInput input;
    input.camera = &camera;
    input.visible_set = &visible;
    input.resources = &resources;
    input.settings = painter.getSettings();

    RenderFrameOutput output = painter.render(input);

    ASSERT_MSG(output.framebuffer != nullptr, "output framebuffer should not be null");
    Framebuffer& outFb = *output.framebuffer;
    ASSERT_EQ(outFb.width(), 800);
    ASSERT_EQ(outFb.height(), 600);

    RGBA skyPixel = outFb.getPixel(400, 100);
    ASSERT_MSG(skyPixel.r != 0 || skyPixel.g != 0 || skyPixel.b != 0,
               "sky should produce non-black pixels");

    RGBA skyBottom = outFb.getPixel(400, 550);
    ASSERT_MSG(skyBottom.r != 0 || skyBottom.g != 0 || skyBottom.b != 0,
               "sky bottom should have non-black pixels");

    ASSERT_EQ(output.errors.size(), static_cast<size_t>(0));

    RenderFrameOutput output2 = painter.render(input);
    ASSERT_EQ(output2.errors.size(), static_cast<size_t>(0));

    painter.resize(640, 480);
    RenderFrameInput inputSmall;
    inputSmall.camera = &camera;
    inputSmall.visible_set = &visible;
    inputSmall.resources = &resources;
    inputSmall.settings = painter.getSettings();
    RenderFrameOutput outputSmall = painter.render(inputSmall);
    ASSERT_MSG(outputSmall.framebuffer != nullptr, "resized render should have framebuffer");
    ASSERT_EQ(outputSmall.framebuffer->width(), 640);
    ASSERT_EQ(outputSmall.framebuffer->height(), 480);

    RenderFrameInput nullInput;
    nullInput.camera = nullptr;
    nullInput.visible_set = nullptr;
    nullInput.resources = nullptr;
    nullInput.settings = painter.getSettings();
    RenderFrameOutput nullOutput = painter.render(nullInput);
    ASSERT_MSG(nullOutput.errors.size() > 0, "null input should produce errors");

    painter.shutdown();
    Framebuffer& fbAfter = painter.getFramebuffer();
    ASSERT_EQ(fbAfter.width(), 0);
    ASSERT_EQ(fbAfter.height(), 0);

    painter.initialize(320, 240);
    MockResourceProvider res2;
    VisibleSet vis2;
    RenderFrameInput input3;
    input3.camera = &camera;
    input3.visible_set = &vis2;
    input3.resources = &res2;
    input3.settings = painter.getSettings();
    RenderFrameOutput output3 = painter.render(input3);
    ASSERT_MSG(output3.framebuffer != nullptr, "render with custom settings should work");
    ASSERT_EQ(output3.framebuffer->width(), 320);
    ASSERT_EQ(output3.framebuffer->height(), 240);

    std::cout << "  Painter tests passed!" << std::endl;
    return true;
}
