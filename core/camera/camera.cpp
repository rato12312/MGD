#include "Camera.h"
#include "../common/Mat4.h"
#include <cmath>
#include <algorithm>

namespace mgd {

static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
static constexpr float RAD_TO_DEG = 180.0f / 3.14159265358979323846f;
static constexpr float PITCH_LIMIT = 89.0f * DEG_TO_RAD;
static constexpr Vec3 WORLD_UP = {0.0f, 1.0f, 0.0f};

Camera::Camera() {
    cached_view = Mat4::identity();
    cached_proj = Mat4::identity();
    cached_vp = Mat4::identity();
}

void Camera::recalcView() const {
    Vec3 fwd = getForward();
    Vec3 target = state.position + fwd;
    cached_view = Mat4::lookAt(state.position, target, WORLD_UP);
    dirty_view = false;
    dirty_frustum = true;
}

void Camera::recalcProjection() const {
    float fov_rad = state.fov * DEG_TO_RAD;
    cached_proj = Mat4::perspective(fov_rad, state.aspect_ratio, state.near_plane, state.far_plane);
    dirty_proj = false;
    dirty_frustum = true;
}

void Camera::recalcFrustum() const {
    if (dirty_view) recalcView();
    if (dirty_proj) recalcProjection();
    cached_vp = cached_proj * cached_view;
    cached_frustum.extractFromVP(cached_view, cached_proj);
    dirty_frustum = false;
}

void Camera::setPosition(const Vec3& pos) {
    state.position = pos;
    dirty_view = true;
}

void Camera::setOrientation(const Vec3& pitch_yaw_roll) {
    state.orientation = pitch_yaw_roll;
    dirty_view = true;
}

void Camera::lookAt(const Vec3& target) {
    Vec3 dir = (target - state.position).normalized();
    state.orientation.x = std::asin(dir.y);
    state.orientation.y = std::atan2(dir.x, dir.z);
    state.orientation.z = 0.0f;
    dirty_view = true;
}

void Camera::setFOV(float degrees) {
    state.fov = degrees;
    dirty_proj = true;
}

void Camera::setAspectRatio(float w_h) {
    state.aspect_ratio = w_h;
    dirty_proj = true;
}

void Camera::setNearPlane(float n) {
    state.near_plane = n;
    dirty_proj = true;
}

void Camera::setFarPlane(float f) {
    state.far_plane = f;
    dirty_proj = true;
}

void Camera::setViewDistance(float d) {
    state.view_distance = d;
}

const CameraState& Camera::getState() const {
    return state;
}

void Camera::moveForward(float amount) {
    Vec3 fwd = getForward();
    state.position += fwd * amount;
    dirty_view = true;
}

void Camera::moveBackward(float amount) {
    moveForward(-amount);
}

void Camera::moveRight(float amount) {
    Vec3 right = getRight();
    state.position += right * amount;
    dirty_view = true;
}

void Camera::moveLeft(float amount) {
    moveRight(-amount);
}

void Camera::moveUp(float amount) {
    state.position += WORLD_UP * amount;
    dirty_view = true;
}

void Camera::moveDown(float amount) {
    moveUp(-amount);
}

void Camera::rotateYaw(float radians) {
    state.orientation.y += radians;
    dirty_view = true;
}

void Camera::rotatePitch(float radians) {
    state.orientation.x += radians;
    if (state.orientation.x > PITCH_LIMIT) state.orientation.x = PITCH_LIMIT;
    if (state.orientation.x < -PITCH_LIMIT) state.orientation.x = -PITCH_LIMIT;
    dirty_view = true;
}

void Camera::rotateRoll(float radians) {
    state.orientation.z += radians;
    dirty_view = true;
}

Vec3 Camera::getForward() const {
    float pitch = state.orientation.x;
    float yaw = state.orientation.y;
    return {
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::cos(yaw)
    };
}

Vec3 Camera::getRight() const {
    Vec3 fwd = getForward();
    Vec3 right = fwd.cross(WORLD_UP);
    float len = right.length();
    if (len < 1e-8f) return {1.0f, 0.0f, 0.0f};
    return right / len;
}

Vec3 Camera::getUp() const {
    Vec3 right = getRight();
    Vec3 fwd = getForward();
    return right.cross(fwd);
}

const Mat4& Camera::getViewMatrix() const {
    if (dirty_view) recalcView();
    return cached_view;
}

const Mat4& Camera::getProjectionMatrix() const {
    if (dirty_proj) recalcProjection();
    return cached_proj;
}

const Mat4& Camera::getViewProjectionMatrix() const {
    if (dirty_frustum) recalcFrustum();
    return cached_vp;
}

const Frustum& Camera::getFrustum() const {
    if (dirty_frustum) recalcFrustum();
    return cached_frustum;
}

Camera::ScreenProjection Camera::projectWorldToScreen(const Vec3& world_pos, int screen_w, int screen_h) {
    const Mat4& vp = getViewProjectionMatrix();

    Vec4 clip = vp.transformPoint(Vec4(world_pos, 1.0f));
    ScreenProjection result;

    if (clip.w <= 0.0f) {
        result.screen_x = 0;
        result.screen_y = 0;
        result.depth = 0;
        result.inside_frustum = false;
        return result;
    }

    Vec4 ndc = clip.perspectiveDivide();
    result.screen_x = (ndc.x + 1.0f) * 0.5f * static_cast<float>(screen_w);
    result.screen_y = (1.0f - ndc.y) * 0.5f * static_cast<float>(screen_h);
    result.depth = ndc.z;
    result.inside_frustum = (ndc.x >= -1.0f && ndc.x <= 1.0f &&
                             ndc.y >= -1.0f && ndc.y <= 1.0f &&
                             ndc.z >= 0.0f && ndc.z <= 1.0f);
    return result;
}

Ray Camera::screenToWorldRay(float screen_x, float screen_y, int screen_w, int screen_h) {
    const Mat4& vp = getViewProjectionMatrix();
    auto inv_vp = vp.inverse();
    if (!inv_vp) {
        return Ray(state.position, getForward());
    }

    float ndc_x = (2.0f * screen_x / static_cast<float>(screen_w)) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / static_cast<float>(screen_h));

    Vec4 near4 = inv_vp->transformPoint(Vec4(ndc_x, ndc_y, 0.0f, 1.0f));
    Vec4 far4 = inv_vp->transformPoint(Vec4(ndc_x, ndc_y, 1.0f, 1.0f));

    Vec3 near_pt = near4.perspectiveDivide().xyz();
    Vec3 far_pt = far4.perspectiveDivide().xyz();

    return Ray(near_pt, far_pt - near_pt);
}

Camera::DebugInfo Camera::getDebugInfo() const {
    getViewMatrix();
    DebugInfo info;
    info.position = state.position;
    info.forward = getForward();
    info.right = getRight();
    info.up = getUp();
    info.fov = state.fov;
    info.aspect = state.aspect_ratio;
    info.near_p = state.near_plane;
    info.far_p = state.far_plane;
    return info;
}

} // namespace mgd
