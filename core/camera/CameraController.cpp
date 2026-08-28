#include "CameraController.h"
#include "Camera.h"
#include <cmath>
#include <algorithm>

namespace mgd {

static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
static constexpr float PITCH_LIMIT = 89.0f * DEG_TO_RAD;

void CameraController::update(Camera& camera, float dt) {
    camera.rotateYaw(input.yaw_delta * sensitivity);
    camera.rotatePitch(input.pitch_delta * sensitivity);
    camera.rotateRoll(input.roll_delta * sensitivity);

    float move_speed = input.speed * speed * dt;

    if (input.forward != 0.0f) {
        camera.moveForward(input.forward * move_speed);
    }
    if (input.strafe != 0.0f) {
        camera.moveRight(input.strafe * move_speed);
    }
    if (input.vertical != 0.0f) {
        camera.moveUp(input.vertical * move_speed);
    }
}

void CameraController::setInput(const CameraInput& in) {
    input = in;
}

void CameraController::setSensitivity(float s) {
    sensitivity = s;
}

void CameraController::setSpeed(float s) {
    speed = s;
}

} // namespace mgd
