#pragma once

#include "../common/Types.h"
#include "../common/RGBA.h"

namespace mgd {

struct RenderSettings {
    int width = 800;
    int height = 600;
    int target_fps = 30;
    TextureFilter texture_filter = TextureFilter::NEAREST;
    bool backface_culling = true;
    bool depth_test = true;
    bool lighting_enabled = true;
    bool shadows_enabled = false;
    DebugMode debug_mode = DebugMode::NORMAL;
    RGBA clear_color = {20, 20, 40, 255};
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
};

} // namespace mgd
