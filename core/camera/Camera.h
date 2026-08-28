#pragma once

#include "CameraState.h"
#include "../common/Mat4.h"
#include "../common/Frustum.h"
#include "../common/Ray.h"
#include <cstdint>

namespace mgd {

class Camera {
    CameraState state;

    mutable bool dirty_view = true;
    mutable bool dirty_proj = true;
    mutable bool dirty_frustum = true;
    mutable Mat4 cached_view;
    mutable Mat4 cached_proj;
    mutable Mat4 cached_vp;
    mutable Frustum cached_frustum;

    void recalcView() const;
    void recalcProjection() const;
    void recalcFrustum() const;

public:
    Camera();

    void setPosition(const Vec3& pos);
    void setOrientation(const Vec3& pitch_yaw_roll);
    void lookAt(const Vec3& target);
    void setFOV(float degrees);
    void setAspectRatio(float w_h);
    void setNearPlane(float n);
    void setFarPlane(float f);
    void setViewDistance(float d);

    const CameraState& getState() const;

    void moveForward(float amount);
    void moveBackward(float amount);
    void moveLeft(float amount);
    void moveRight(float amount);
    void moveUp(float amount);
    void moveDown(float amount);

    void rotateYaw(float radians);
    void rotatePitch(float radians);
    void rotateRoll(float radians);

    Vec3 getForward() const;
    Vec3 getRight() const;
    Vec3 getUp() const;

    const Mat4& getViewMatrix() const;
    const Mat4& getProjectionMatrix() const;
    const Mat4& getViewProjectionMatrix() const;
    const Frustum& getFrustum() const;

    struct ScreenProjection {
        float screen_x, screen_y;
        float depth;
        bool inside_frustum;
    };
    ScreenProjection projectWorldToScreen(const Vec3& world_pos, int screen_w, int screen_h);
    Ray screenToWorldRay(float screen_x, float screen_y, int screen_w, int screen_h);

    struct DebugInfo {
        Vec3 position;
        Vec3 forward;
        Vec3 right;
        Vec3 up;
        float fov;
        float aspect;
        float near_p;
        float far_p;
    };
    DebugInfo getDebugInfo() const;
};

} // namespace mgd
