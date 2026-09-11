// FSR 2.x TAA (Temporal Anti-Aliasing) Compute Shader
// Temporal Anti-Aliasing with motion vectors, disocclusion handling, and history blending

#include "Fsr2Taa.h"
#include <cmath>

namespace mgd {
namespace gpu {

// TAA Push Constants
struct TaaPushConstants {
    // Input dimensions
    uint32_t input_width;
    uint32_t input_height;
    
    // Output dimensions
    uint32_t output_width;
    uint32_t output_height;
    
    // Scale factors
    float scale_x;
    float scale_y;
    
    // TAA parameters
    float temporal_alpha;           // History blend factor (0.9 typical)
    float motion_vector_scale_x;    // Motion vector scale
    float motion_vector_scale_y;    // Motion vector scale
    float disocclusion_threshold;   // Depth-based disocclusion
    float motion_threshold;         // Motion vector magnitude threshold
    float color_threshold;          // Color difference threshold for history rejection
    float max_velocity;             // Max velocity for clamping
    float history_weight;           // History blend weight
    
    // Jitter
    float jitter_x;
    float jitter_y;
    float prev_jitter_x;
    float prev_jitter_y;
    
    // Frame index
    uint32_t frame_index;
    
    // TAA parameters
    float temporal_alpha;
    float disocclusion_threshold;
    float motion_threshold;
    float color_threshold;
    float max_velocity;
    float history_weight;
    float new_frame_weight;
    float jitter_x;
    float jitter_y;
    float prev_jitter_x;
    float prev_jitter_y;
    uint32_t frame_index;
    
    // Mode: 0=full FSR2, 1=EASU only, 2=RCAS only, 3=TAA only
    int mode;
    
    // Padding
    float pad0;
    float pad1;
    float pad2;
};

} // namespace gpu
} // namespace mgd

// GLSL Compute Shader for FSR 2.x TAA (Temporal Anti-Aliasing)
// Based on AMD FidelityFX FSR 2.x reference implementation
/*
#version 460 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D current_color;      // Current frame color (EASU output)
layout(set = 0, binding = 1) uniform sampler2D history_color;      // Previous frame color (reprojected)
layout(set = 0, binding = 2) uniform sampler2D motion_vectors;     // Motion vectors (RG16F)
layout(set = 0, binding = 3) uniform sampler2D depth_buffer;       // Current depth
layout(set = 0, binding = 3) uniform sampler2D prev_depth_buffer;  // Previous depth
layout(set = 0, binding = 4) uniform sampler2D prev_color;         // Previous frame color (raw)
layout(set = 0, binding = 4) uniform sampler2D prev_obj_id;        // Previous object ID

layout(set = 0, binding = 5, r32f) uniform image2D output_image;   // Output image

layout(push_constant) uniform TaaConstants {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    float scale_x;
    float scale_y;
    float motion_vector_scale_x;
    float motion_vector_scale_y;
    float disocclusion_threshold;
    float motion_threshold;
    float color_threshold;
    float max_velocity;
    float history_weight;
    float jitter_x;
    float jitter_y;
    float prev_jitter_x;
    float prev_jitter_y;
    uint32_t frame_index;
    float temporal_alpha;
    float disocclusion_threshold;
    float motion_threshold;
    float color_threshold;
    float max_velocity;
    float history_weight;
    float new_frame_weight;
    float jitter_x;
    float jitter_y;
    float prev_jitter_x;
    float prev_jitter_y;
    uint32_t frame_index;
    int mode;
    float pad0;
    float pad1;
    float pad2;
} push;

layout(set = 0, binding = 0) uniform sampler2D current_color;
layout(set = 0, binding = 1) uniform sampler2D history_color;
layout(set = 0, binding = 2) uniform sampler2D motion_vectors;
layout(set = 0, binding = 3) uniform sampler2D depth_buffer;
layout(set = 0, binding = 4) uniform sampler2D prev_depth_buffer;
layout(set = 0, binding = 5) uniform sampler2D prev_color;
layout(set = 0, binding = 6) uniform sampler2D prev_obj_id;
layout(set = 0, binding = 6, r32f) uniform image2D output_image;

void main() {
    uvec2 gid = gl_GlobalInvocationID.xy;
    uvec2 output_size = uvec2(push.output_width, push.output_height);
    
    if (gid.x >= push.output_width || gid.y >= push.output_height) {
        return;
    }
    
    // Current pixel UV
    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(push.output_width, push.output_height);
    
    // Sample current frame color
    vec3 current_color = texture(current_color, vec2(gl_GlobalInvocationID.xy) / vec2(push.output_width, push.output_height)).rgb;
    
    // Read motion vector
    vec2 motion = texture(motion_vectors, vec2(gl_GlobalInvocationID.xy) / vec2(push.output_width, push.output_height)).rg;
    motion = motion * vec2(push.motion_vector_scale_x, push.motion_vector_scale_y);
    
    // Reproject previous frame position
    vec2 prev_uv = uv - motion * vec2(push.scale_x, push.scale_y);
    
    // Clamp to valid range
    prev_uv = clamp(prev_uv, vec2(0.0), vec2(1.0));
    
    // Sample history color
    vec3 history_color = texture(history_color, prev_uv).rgb;
    
    // Sample current depth
    float current_depth = texture(depth_buffer, vec2(gl_GlobalInvocationID.xy) / vec2(push.output_width, push.output_height)).r;
    
    // Sample previous depth at reprojected position
    float prev_depth = texture(prev_depth_buffer, prev_uv).r;
    
    // Disocclusion detection: depth difference
    float depth_diff = abs(current_depth - prev_depth);
    float disocclusion = step(push.disocclusion_threshold, depth_diff);
    
    // Motion magnitude check
    float motion_magnitude = length(motion);
    float motion_factor = step(push.motion_threshold, motion_magnitude);
    
    // Color difference for history rejection
    vec3 prev_color = texture(prev_color, prev_uv).rgb;
    float color_diff = length(current_color - prev_color);
    float color_reject = step(push.color_threshold, color_diff);
    
    // Combine rejection factors
    float reject = max(disocclusion, max(motion_factor, color_reject));
    float accept = 1.0 - reject;
    
    // Sample history color at reprojected position
    vec3 history_color = texture(history_color, prev_uv).rgb;
    
    // Velocity clamping
    float velocity = length(motion);
    float velocity_clamp = clamp(velocity / push.max_velocity, 0.0, 1.0);
    
    // Temporal blend factor
    float blend_factor = push.temporal_alpha * accept;
    
    // History weight based on confidence
    float history_confidence = accept * (1.0 - velocity_clamp * 0.5);
    float temporal_alpha = push.temporal_alpha * history_confidence;
    
    // Temporal blend
    vec3 blended = mix(current_color, history_color, temporal_alpha);
    
    // Variance clipping (YCoCg color space for better clamping)
    vec3 ycocg_current = vec3(
        dot(current_color, vec3(0.25, 0.5, 0.25)),
        dot(current_color, vec3(0.5, -0.5, 0.0)),
        dot(current_color, vec3(0.25, -0.5, 0.25))
    );
    
    vec3 ycocg_history = vec3(
        dot(history_color, vec3(0.25, 0.5, 0.25)),
        dot(history_color, vec3(0.5, -0.5, 0.0)),
        dot(history_color, vec3(0.25, -0.5, 0.25))
    );
    
    // Variance clipping in YCoCg space
    float variance = 0.0;
    // Simplified variance estimation
    
    // Clamp history to current pixel neighborhood
    vec3 min_color = current_color;
    vec3 max_color = current_color;
    
    // Sample 3x3 neighborhood for variance clipping
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            ivec2 offset = ivec2(dx, dy);
            vec2 sample_uv = vec2(gl_GlobalInvocationID.xy) / vec2(push.output_width, push.output_height) + vec2(offset) / vec2(push.output_width, push.output_height);
            vec3 neighbor = texture(current_color, clamp(sample_uv, vec2(0.0), vec2(1.0))).rgb;
            min_color = min(min_color, neighbor);
            max_color = max(max_color, neighbor);
        }
        
        // Clamp history color to neighborhood bounds
        vec3 clamped_history = clamp(history_color, min_color, max_color);
        
        // Final blend
        vec3 final_color = mix(current_color, clamped_history, temporal_alpha);
        
        // Write output
        imageStore(output_image, ivec2(gl_GlobalInvocationID.xy), vec4(final_color, 1.0));
    }
}

*/