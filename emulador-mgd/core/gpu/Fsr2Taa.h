#pragma once

// FSR 2.x TAA (Temporal Anti-Aliasing) Compute Shader
// Temporal Anti-Aliasing with motion vectors, disocclusion handling, and history blending

#include <vulkan/vulkan.h>
#include <cstdint>

namespace mgd {
namespace gpu {

// Push constants para TAA
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
    float disocclusion_threshold;   // Depth-based disocclusion threshold
    float motion_threshold;         // Motion vector magnitude threshold
    float color_threshold;          // Color difference threshold for history rejection
    float max_velocity;             // Max velocity for clamping
    float padding[3];
};

// FSR 2.x TAA Compute Pipeline
class Fsr2Taa {
public:
    Fsr2Taa() = default;
    ~Fsr2Taa() = default;

    bool init(VulkanContext* ctx);
    void shutdown();
    
    // Execute TAA compute shader
    bool execute(VkCommandBuffer cb, const TaaPushConstants& push, 
                 VkImageView input, VkImageView history, VkImageView motion_vectors,
                 VkImageView depth, VkImageView output);
    
    // Pipeline access
    VkPipeline getPipeline() const { return pipeline_; }
    VkPipelineLayout getLayout() const { return layout_; }

private:
    VulkanContext* ctx_ = nullptr;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    VkShaderModule cs_module_ = VK_NULL_HANDLE;
};

} // namespace gpu
} // namespace mgd