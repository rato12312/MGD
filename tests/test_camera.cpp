#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/camera/Camera.h"

using namespace mgd;

static constexpr float PI = 3.14159265358979323846f;

bool run_camera_tests() {
    Camera cam;
    const CameraState& s = cam.getState();
    ASSERT_NEAR(s.position.x, 0.0f, 0.001f);
    ASSERT_NEAR(s.position.y, 0.0f, 0.001f);
    ASSERT_NEAR(s.position.z, 5.0f, 0.001f);
    ASSERT_NEAR(s.fov, 70.0f, 0.001f);
    ASSERT_NEAR(s.near_plane, 0.1f, 0.001f);
    ASSERT_NEAR(s.far_plane, 1000.0f, 0.001f);

    Vec3 fwd = cam.getForward();
    ASSERT_NEAR(fwd.x, 0.0f, 0.01f);
    ASSERT_NEAR(fwd.y, 0.0f, 0.01f);
    ASSERT_NEAR(fwd.z, 1.0f, 0.01f);

    cam.setPosition(Vec3(0, 0, 0));
    cam.lookAt(Vec3(0, 0, -10));
    Vec3 fwdAfter = cam.getForward();
    ASSERT_NEAR(fwdAfter.z, -1.0f, 0.05f);
    ASSERT_NEAR(std::abs(fwdAfter.x), 0.05f, 0.05f);
    ASSERT_NEAR(fwdAfter.y, 0.0f, 0.05f);

    cam.setFOV(90.0f);
    ASSERT_NEAR(cam.getState().fov, 90.0f, 0.001f);

    cam.setPosition(Vec3(0, 0, 0));
    cam.setOrientation(Vec3(0, 0, 0));
    cam.rotateYaw(PI);
    Vec3 fwdYaw = cam.getForward();
    ASSERT_NEAR(fwdYaw.x, 0.0f, 0.05f);
    ASSERT_NEAR(fwdYaw.z, -1.0f, 0.05f);

    cam.setPosition(Vec3(0, 0, 10));
    cam.setOrientation(Vec3(0, 0, 0));
    auto sp = cam.projectWorldToScreen(Vec3(0, 0, -10), 800, 600);
    ASSERT_MSG(sp.inside_frustum == false, "point behind camera should not be in frustum");

    cam.setPosition(Vec3(0, 0, 0));
    cam.setOrientation(Vec3(0, 0, 0));
    cam.setFOV(90.0f);
    cam.setAspectRatio(800.0f / 600.0f);
    auto spFront = cam.projectWorldToScreen(Vec3(0, 0, 10), 800, 600);
    ASSERT_MSG(spFront.inside_frustum == true, "point in front should be in frustum");
    ASSERT_NEAR(spFront.screen_x, 400.0f, 5.0f);
    ASSERT_NEAR(spFront.screen_y, 300.0f, 5.0f);

    Ray ray = cam.screenToWorldRay(400.0f, 300.0f, 800, 600);
    ASSERT_MSG(ray.direction.length() > 0.9f, "ray direction should be normalized-ish");
    ASSERT_NEAR(ray.direction.z, 1.0f, 0.15f);

    cam.setPosition(Vec3(0, 0, 10));
    cam.setOrientation(Vec3(0, 0, 0));
    cam.moveForward(5.0f);
    ASSERT_NEAR(s.position.z, 15.0f, 0.01f);

    cam.moveBackward(5.0f);
    ASSERT_NEAR(s.position.z, 10.0f, 0.01f);

    cam.moveRight(3.0f);
    ASSERT_NEAR(s.position.x, -3.0f, 0.01f);

    cam.moveLeft(3.0f);
    ASSERT_NEAR(s.position.x, 0.0f, 0.01f);

    cam.moveUp(2.0f);
    ASSERT_NEAR(s.position.y, 2.0f, 0.01f);

    cam.moveDown(2.0f);
    ASSERT_NEAR(s.position.y, 0.0f, 0.01f);

    cam.setNearPlane(0.5f);
    ASSERT_NEAR(s.near_plane, 0.5f, 0.001f);

    cam.setFarPlane(500.0f);
    ASSERT_NEAR(s.far_plane, 500.0f, 0.001f);

    cam.setAspectRatio(2.0f);
    ASSERT_NEAR(s.aspect_ratio, 2.0f, 0.001f);

    const Mat4& viewMat = cam.getViewMatrix();
    ASSERT_MSG(viewMat.m[15] == 1.0f, "view matrix should be valid");

    const Mat4& projMat = cam.getProjectionMatrix();
    ASSERT_MSG(projMat.m[15] == 0.0f, "proj matrix w should be 0");

    const Frustum& frustum = cam.getFrustum();
    ASSERT_MSG(frustum.planes[Frustum::NEAR].normal.length() > 0.0f, "near plane should have non-zero normal");

    auto debugInfo = cam.getDebugInfo();
    ASSERT_NEAR(debugInfo.fov, s.fov, 0.001f);
    ASSERT_NEAR(debugInfo.near_p, s.near_plane, 0.001f);
    ASSERT_NEAR(debugInfo.far_p, s.far_plane, 0.001f);

    cam.rotatePitch(0.5f);
    ASSERT_NEAR(s.orientation.x, 0.5f, 0.01f);

    cam.rotateRoll(0.3f);
    ASSERT_NEAR(s.orientation.z, 0.3f, 0.01f);

    std::cout << "  Camera tests passed!" << std::endl;
    return true;
}
