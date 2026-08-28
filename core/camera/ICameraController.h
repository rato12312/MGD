#pragma once

namespace mgd {

class Camera;

struct CameraInput {
    float forward = 0;    // -1 to 1
    float strafe = 0;     // -1 to 1
    float vertical = 0;   // -1 to 1
    float yaw_delta = 0;
    float pitch_delta = 0;
    float roll_delta = 0;
    float speed = 5.0f;
    float sensitivity = 0.003f;
};

class ICameraController {
public:
    virtual ~ICameraController() = default;
    virtual void update(Camera& camera, float dt) = 0;
};

} // namespace mgd
