#pragma once

#include "../common/Vec3.h"

namespace mgd {

struct CameraState {
    Vec3 position = {0, 0, 5};
    Vec3 orientation = {0, 0, 0}; // pitch, yaw, roll in radians
    float fov = 70.0f;            // degrees
    float aspect_ratio = 16.0f / 9.0f;
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    float view_distance = 500.0f;
};

} // namespace mgd
