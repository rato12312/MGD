// FSR 1.0 (FidelityFX Super Resolution) implementation for Painter compute
// EASU (Edge-Adaptive Spatial Upsampling) + RCAS (Robust Contrast-Adaptive Sharpening)
// CPU implementation for reference - compute shader version would use similar logic

#include "Fsr10.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace mgd {
namespace gpu {

// ========== FSR 1.0 EASU (Edge-Adaptive Spatial Upsampling) ==========

namespace {
    // Lanczos2 kernel weights (precomputed for 2x scale)
    static const float LANCZOS2_WEIGHTS[4] = {
        0.0f,  // Will be computed at runtime
        0.0f,
        0.0f,
        0.0f
    };
    
    // Lanczos2 kernel function
    inline float lanczos2(float x) {
        if (x == 0.0f) return 1.0f;
        if (x < -2.0f || x > 2.0f) return 0.0f;
        float pix = x * 3.14159265359f;
        float pix2 = pix / 2.0f;
        return (std::sin(pix) * std::sin(pix2)) / (pix * pix2 * 0.5f);
    }
    
    // Edge detection using depth (if available) or color
    inline float computeEdge(const uint8_t* color, int x, int y, int w, int h, int stride) {
        // Simple luminance-based edge detection
        auto lum = [&](int px, int py) -> float {
            px = std::clamp(px, 0, w - 1);
            py = std::clamp(py, 0, h - 1);
            int idx = (py * w + px) * 4;
            return 0.299f * color[idx] + 0.587f * color[idx + 1] + 0.114f * color[idx + 2];
        };
        
        float center = lum(x, y);
        float dx = std::abs(lum(x + 1, y) - lum(x - 1, y));
        float dy = std::abs(lum(x, y + 1) - lum(x, y - 1));
        return std::max(dx, dy);
    }
    
    // Directional weight based on edge direction
    inline void computeDirectionalWeights(float edge_x, float edge_y, float weights[4]) {
        float angle = std::atan2(edge_y, edge_x);
        // 4 directional weights (horizontal, vertical, diagonal1, diagonal2)
        weights[0] = std::cos(angle);      // Horizontal
        weights[1] = std::sin(angle);      // Vertical
        weights[2] = std::cos(angle + 0.785f); // Diagonal 1
        weights[3] = std::sin(angle + 0.785f); // Diagonal 2
    }
}

void fsrEasu(const FsrEasuInput& input, FsrEasuOutput& output, const FsrEasuConstants& constants) {
    // For each output pixel, gather 12-tap Lanczos2 neighborhood from input
    // Apply edge detection and directional weighting
    
    int in_w = input.width;
    int in_h = input.height;
    int out_w = output.width;
    int out_h = output.height;
    
    float scale_x = constants.scale_x;
    float scale_y = constants.scale_y;
    float inv_scale_x = constants.inv_scale_x;
    float inv_scale_y = constants.inv_scale_y;
    
    for (int out_y = 0; out_y < out_h; ++out_y) {
        for (int out_x = 0; out_x < out_w; ++out_x) {
            // Map output coordinate to input space
            float in_x = (out_x + 0.5f) * inv_scale_x - 0.5f;
            float in_y = (out_y + 0.5f) * inv_scale_y - 0.5f;
            
            int base_x = int(std::floor(in_x));
            int base_y = int(std::floor(in_y));
            float frac_x = in_x - base_x;
            float frac_y = in_y - base_y;
            
            // Edge detection at input coordinate
            float edge = computeEdge(input.color, base_x, base_y, in_w, in_h, in_w * 4);
            bool is_edge = edge > constants.edge_threshold;
            
            // 12-tap Lanczos2 gathering (4x4 neighborhood)
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
            float total_weight = 0.0f;
            
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    int sx = std::clamp(base_x + dx, 0, constants.input_width - 1);
                    int sy = std::clamp(base_y + dy, 0, constants.input_height - 1);
                    
                    float wx = lanczos2((sx + 0.5f) * constants.inv_scale_x - (out_x + 0.5f));
                    float wy = lanczos2((sy + 0.5f) * constants.inv_scale_y - (out_y + 0.5f));
                    float weight = wx * wy;
                    
                    if (weight > 0.0f) {
                        int idx = (sy * in_w + sx) * 4;
                        float w = weight;
                        
                        // Edge-adaptive weighting
                        if (is_edge) {
                            // Reduce weight across edges
                            weight *= 0.5f;
                        }
                        
                        r += input.color[idx] * weight;
                        g += input.color[idx + 1] * weight;
                        b += input.color[idx + 2] * weight;
                        a += input.color[idx + 3] * weight;
                        total_weight += weight;
                    }
                }
            }
            
            if (total_weight > 0.0f) {
                r /= total_weight;
                g /= total_weight;
                b /= total_weight;
                a /= total_weight;
            }
            
            int out_idx = (out_y * out_w + out_x) * 4;
            output.color[out_idx] = uint8_t(std::clamp(r, 0.0f, 255.0f));
            output.color[out_idx + 1] = uint8_t(std::clamp(g, 0.0f, 255.0f));
            output.color[out_idx + 2] = uint8_t(std::clamp(b, 0.0f, 255.0f));
            output.color[out_idx + 3] = uint8_t(std::clamp(a, 0.0f, 255.0f));
        }
    }
}

// ========== FSR 1.0 RCAS (Robust Contrast-Adaptive Sharpening) ==========

void fsrRcas(const FsrRcasInput& input, FsrRcasOutput& output, const FsrRcasConstants& constants) {
    // RCAS: Local contrast adaptive sharpening
    // For each pixel, compute local contrast and apply adaptive sharpening
    
    int w = input.width;
    int h = input.height;
    float sharpness = std::clamp(constants.sharpness, 0.0f, 1.0f);
    
    // RCAS parameters from AMD FSR 1.0
    float rcas_sharpness = sharpness * 0.5f; // Scale to internal range
    
    // Copy input to output first
    std::memcpy(output.color, input.color, w * h * 4);
    
    // RCAS: 5-tap cross filter for local contrast
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            int idx = (y * w + x) * 4;
            
            // Center pixel luminance
            float center_lum = 0.299f * input.color[idx] + 
                              0.587f * input.color[idx + 1] + 
                              0.114f * input.color[idx + 2];
            
            // 4-neighbor luminance
            float neighbor_lum[4];
            neighbor_lum[0] = 0.299f * input.color[(y * w + x - 1) * 4] + 
                             0.587f * input.color[(y * w + x - 1) * 4 + 1] + 
                             0.114f * input.color[(y * w + x - 1) * 4 + 2]; // left
            neighbor_lum[1] = 0.299f * input.color[(y * w + x + 1) * 4] + 
                             0.587f * input.color[(y * w + x + 1) * 4 + 1] + 
                             0.114f * input.color[(y * w + x + 1) * 4 + 2]; // right
            neighbor_lum[2] = 0.299f * input.color[((y - 1) * w + x) * 4] + 
                             0.587f * input.color[((y - 1) * w + x) * 4 + 1] + 
                             0.114f * input.color[((y - 1) * w + x) * 4 + 2]; // up
            neighbor_lum[3] = 0.299f * input.color[((y + 1) * w + x) * 4] + 
                             0.587f * input.color[((y + 1) * w + x) * 4 + 1] + 
                             0.114f * input.color[((y + 1) * w + x) * 4 + 2]; // down
            
            // Local contrast
            float min_lum = center_lum;
            float max_lum = center_lum;
            for (int i = 0; i < 4; ++i) {
                min_lum = std::min(min_lum, neighbor_lum[i]);
                max_lum = std::max(max_lum, neighbor_lum[i]);
            }
            float contrast = max_lum - min_lum;
            
            // Adaptive sharpening based on local contrast
            float sharp = rcas_sharpness * std::min(contrast * 4.0f, 1.0f);
            
            // Apply sharpening to RGB
            for (int c = 0; c < 3; ++c) {
                float center = input.color[idx + c];
                float sum = 0.0f;
                for (int i = 0; i < 4; ++i) {
                    int n_idx = ((y + (i < 2 ? (i == 0 ? -1 : 1) : 0)) * w + 
                                (x + (i >= 2 ? (i == 2 ? -1 : 1) : 0))) * 4 + c;
                    sum += input.color[n_idx];
                }
                float avg = sum / 4.0f;
                float sharpened = center + sharp * (center - avg);
                output.color[idx + c] = uint8_t(std::clamp(sharpened, 0.0f, 255.0f));
            }
        }
    }
}

// Combined FSR 1.0 pipeline
void fsr1Pipeline(const uint8_t* input_color, uint32_t in_w, uint32_t in_h,
                  uint8_t* output_color, uint32_t out_w, uint32_t out_h,
                  float sharpness) {
    // Allocate intermediate buffer for EASU output
    std::vector<uint8_t> easu_output(out_w * out_h * 4);
    
    // EASU constants
    FsrEasuConstants easu_const;
    easu_const.input_width = in_w;
    easu_const.input_height = in_h;
    easu_const.output_width = out_w;
    easu_const.output_height = out_h;
    easu_const.scale_x = float(out_w) / in_w;
    easu_const.scale_y = float(out_h) / in_h;
    easu_const.inv_scale_x = float(in_w) / out_w;
    easu_const.inv_scale_y = float(in_h) / out_h;
    easu_const.texel_size_x = 1.0f / in_w;
    easu_const.texel_size_y = 1.0f / in_h;
    easu_const.edge_threshold = 0.05f;
    easu_const.edge_threshold_min = 0.01f;
    easu_const.edge_threshold_max = 0.2f;
    
    // EASU input/output
    FsrEasuInput easu_in{input_color, nullptr, in_w, in_h};
    FsrEasuOutput easu_out{easu_output.data(), out_w, out_h};
    
    // Run EASU
    fsrEasu(easu_in, easu_out, easu_const);
    
    // RCAS constants
    FsrRcasConstants rcas_const;
    rcas_const.sharpness = sharpness;
    rcas_const.scale_x = 1.0f;
    rcas_const.scale_y = 1.0f;
    
    // RCAS input/output
    FsrRcasInput rcas_in{easu_output.data(), out_w, out_h, sharpness};
    FsrRcasOutput rcas_out{output_color, out_w, out_h};
    
    // Run RCAS
    fsrRcas(rcas_in, rcas_out, rcas_const);
}

} // namespace gpu
} // namespace mgd