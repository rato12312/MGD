#pragma once

#include "ICameraController.h"

namespace mgd {

class CameraController : public ICameraController {
    CameraInput input;
    float sensitivity = 0.003f;
    float speed = 5.0f;

public:
    void update(Camera& camera, float dt) override;
    void setInput(const CameraInput& in);
    void setSensitivity(float s);
    void setSpeed(float s);
};

} // namespace mgd
