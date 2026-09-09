#pragma once

// FSR 1.0 (FidelityFX Super Resolution) implementation for Painter compute
// EASU (Edge-Adaptive Spatial Upsampling) + RCAS (Robust Contrast-Adaptive Sharpening)
// Based on AMD FidelityFX FSR 1.0 reference implementation

#include <cstdint>

namespace mgd {
namespace gpu {

// FSR 1.0 EASU constants
struct FsrEasuConstants {
    // Input/output dimensions
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    
    // Scaling factors
    float scale_x;
    float scale_y;
    float inv_scale_x;
    float inv_scale_y;
    
    // Texture size for sampling
    float texel_size_x;
    float texel_size_y;
    
    // Edge detection thresholds
    float edge_threshold;
    float edge_threshold_min;
    float edge_threshold_max;
};

// FSR 1.0 RCAS constants
struct FsrRcasConstants {
    float sharpness; // 0.0 to 1.0
    float scale_x;
    float scale_y;
};

// FSR 1.0 EASU - Edge-Adaptive Spatial Upsampling
// Input: low-res color (288p), low-res depth (optional)
// Output: high-res color (720p)
struct FsrEasuInput {
    const uint8_t* color;      // RGBA8, 288p
    const uint8_t* depth;      // D16, 288p (optional)
    uint32_t width;
    uint32_t height;
};

struct FsrEasuOutput {
    uint8_t* color;            // RGBA8, 720p
    uint32_t width;
    uint32_t height;
};

// FSR 1.0 RCAS - Robust Contrast-Adaptive Sharpening
// Input: high-res color (720p)
// Output: sharpened high-res color (720p)
struct FsrRcasInput {
    const uint8_t* color;      // RGBA8, 720p
    uint32_t width;
    uint32_t height;
    float sharpness;           // 0.0 to 1.0
};

struct FsrRcasOutput {
    uint8_t* color;            // RGBA8, 720p
    uint32_t width;
    uint32_t height;
};

// EASU: Edge-Adaptive Spatial Upsampling (288p -> 720p)
// Uses 12-tap Lanczos + edge detection + directional interpolation
void fsrEasu(const FsrEasuInput& input, FsrEasuOutput& output, const FsrEasuConstants& constants);

// RCAS: Robust Contrast-Adaptive Sharpening
// Input: already upscaled 720p image, Output: sharpened 720p
void fsrRcas(const FsrRcasInput& input, FsrRcasOutput& output, const FsrRcasConstants& constants);

// Combined FSR 1.0 pipeline: 288p -> 720p (EASU) -> sharpened 720p (RCAS)
void fsr1Pipeline(const uint8_t* input_color, uint32_t in_w, uint32_t in_h,
                  uint8_t* output_color, uint32_t out_w, uint32_t out_h,
                  float sharpness = 0.5f);

} // namespace gpu
} // namespace mgd