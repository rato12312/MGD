#pragma once

// FSR 2.x EASU (Edge-Adaptive Spatial Upsampling) Compute Shader
// Based on AMD FidelityFX FSR 2.x reference implementation
// 12-tap Lanczos2 with edge-adaptive weighting

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace mgd {
namespace gpu {

// EASU Push Constants
struct EasuPushConstants {
    // Input/Output dimensions
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    
    // Scale factors
    float scale_x;
    float scale_y;
    float inv_scale_x;
    float inv_scale_y;
    
    // Jitter for TAA
    float jitter_x;
    float jitter_y;
    
    // FSR 2.0 parameters
    float sharpness;
    float edge_threshold;
    float edge_threshold_min;
    float edge_threshold_max;
    
    // Jitter for current frame
    float jitter_x;
    float jitter_y;
    
    // Frame index for jitter sequence
    uint32_t frame_index;
    
    // Sharpness
    float sharpness;
    
    // Padding for alignment
    float padding[3];
};

class Fsr2EasuCompute {
public:
    Fsr2EasuCompute() = default;
    ~Fsr2EasuCompute() { shutdown(); }

    bool init(VulkanContext* ctx, FramebufferManager* fb_mgr);
    void shutdown();

    // Execute EASU upscaling: rough (input) -> upscaled (output)
    bool execute(const FramebufferManager::FrameHistory* rough_history,
                 const FramebufferManager::FrameHistory* prev_history,
                 VkCommandBuffer cmd);

private:
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    bool createPipelines(VulkanContext* ctx);

    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule cs_module_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule cs_module_ = VK_NULL_HANDLE;
    VkBuffer push_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory push_memory_ = VK_NULL_HANDLE;
    
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    bool createPipelines(VulkanContext* ctx);

    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    bool createPipelines(VulkanContext* ctx);
    
    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule cs_module_ = VK_NULL_HANDLE;
    VkBuffer push_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory push_memory_ = VK_NULL_HANDLE;
    
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    bool createPipelines(VulkanContext* ctx);
    
    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    
    VkPipeline pipeline_easu_ = VK_NULL_HANDLE;
    VkPipeline pipeline_rcas_ = VK_NULL_HANDLE;
    VkPipeline pipeline_taa_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule easu_module_ = VK_NULL_HANDLE;
    VkShaderModule rcas_module_ = VK_NULL_HANDLE;
    VkShaderModule taa_module_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule easu_module_ = VK_NULL_HANDLE;
    VkShaderModule rcas_module_ = VK_NULL_HANDLE;
    VkShaderModule taa_module_ = VK_NULL_HANDLE;
    VkBuffer push_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory push_memory_ = VK_NULL_HANDLE;
    
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    bool createPipelines(VulkanContext* ctx);
    
    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    
    VkPipeline pipeline_easu_ = VK_NULL_HANDLE;
    VkPipeline pipeline_rcas_ = VK_NULL_HANDLE;
    VkPipeline pipeline_taa_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule easu_module_ = VK_NULL_HANDLE;
    VkShaderModule rcas_module_ = VK_NULL_HANDLE;
    VkShaderModule taa_module_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule easu_module_ = VK_NULL_HANDLE;
    VkShaderModule rcas_module_ = VK_NULL_HANDLE;
    VkShaderModule taa_module_ = VK_NULL_HANDLE;
    VkBuffer push_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory push_memory_ = VK_NULL_HANDLE;
    
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    bool createPipelines(VulkanContext* ctx);
};

} // namespace gpu
} // namespace mgd