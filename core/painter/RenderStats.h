#pragma once

#include <cstdint>

namespace mgd {

struct RenderStats {
    uint32_t draw_calls = 0;
    uint32_t triangles_submitted = 0;
    uint32_t triangles_clipped = 0;
    uint32_t triangles_rasterized = 0;
    uint32_t pixels_tested = 0;
    uint32_t pixels_written = 0;
    uint32_t texture_samples = 0;
    float frame_time_ms = 0.0f;
    float raster_time_ms = 0.0f;
    float visibility_time_ms = 0.0f;
};

} // namespace mgd
