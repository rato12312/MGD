#pragma once

// FSR 2.x RCAS (Robust Contrast Adaptive Sharpening) Compute Shader
// Based on AMD FidelityFX FSR 2.x reference implementation

#include <vulkan/vulkan.h>
#include <cstdint>

namespace mgd {
namespace gpu {

// RCAS Push Constants
struct RcasPushConstants {
    // Input dimensions
    uint32_t input_width;
    uint32_t input_height;
    
    // Output dimensions
    uint32_t output_width;
    uint32_t output_height;
    
    // RCAS parameters
    float sharpness;
    float scale_x;
    float scale_y;
    
    // Padding
    float padding[3];
};

// FSR 2.x RCAS Compute Pipeline
class Fsr2RcasCompute {
public:
    Fsr2RcasCompute() = default;
    ~Fsr2RcasCompute() = default;

    bool init(VulkanContext* ctx);
    void shutdown();
    
    // Execute RCAS compute shader
    bool execute(VkCommandBuffer cb, const RcasPushConstants& push, 
                 VkImageView input, VkImageView output);
    
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
    VkDescriptorSetLayoutBinding bindings_[3];
};

} // namespace gpu
} // namespace mgd