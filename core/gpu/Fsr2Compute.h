#pragma once

// FSR 2.x Compute Shader Implementation
// EASU (Edge-Adaptive Spatial Upsampling) + RCAS (Robust Contrast-Adaptive Sharpening) + TAA
// Based on AMD FidelityFX FSR 2.x reference implementation

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <cmath>

namespace mgd {
namespace gpu {

// FSR 2.x EASU constants
struct Fsr2EasuConstants {
    // Input/output dimensions
    uint32_t input_width = 512;
    uint32_t input_height = 288;
    uint32_t output_width = 1280;
    uint32_t output_height = 720;
    
    // Scale factors
    float scale_x = 2.5f;
    float scale_y = 2.5f;
    float inv_scale_x = 0.4f;
    float inv_scale_y = 0.4f;
    
    // Jitter for temporal accumulation
    float jitter_x = 0.0f;
    float jitter_y = 0.0f;
    float prev_jitter_x = 0.0f;
    float prev_jitter_y = 0.0f;
    
    // Edge detection
    float edge_threshold = 0.05f;
    float edge_threshold_min = 0.01f;
    float edge_threshold_max = 0.2f;
    
    // Lanczos lobes
    int lanczos_lobes = 2;
    
    // Sharpness
    float sharpness = 0.5f;
};

// FSR 2.x RCAS constants
struct Fsr2RcasConstants {
    float sharpness = 0.5f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
};

// TAA constants
struct TaaConstants {
    float temporal_alpha = 0.9f;           // History blend factor
    float motion_vector_scale_x = 1.0f;
    float motion_vector_scale_y = 1.0f;
    float disocclusion_threshold = 0.1f;   // Depth-based disocclusion
    float motion_threshold = 0.5f;         // Motion vector magnitude threshold
    float color_threshold = 0.1f;          // Color difference threshold for history rejection
    float max_velocity = 128.0f;           // Max velocity for clamping
    float history_weight = 0.9f;           // History blend weight
    float new_frame_weight = 0.1f;         // New frame weight
};

// Push constants for FSR 2.x compute shader
struct Fsr2PushConstants {
    // Input dimensions (rough/rascunho)
    uint32_t input_width = 512;
    uint32_t input_height = 288;
    
    // Output dimensions (final)
    uint32_t output_width = 1280;
    uint32_t output_height = 720;
    
    // Jitter offsets
    float jitter_x = 0.0f;
    float jitter_y = 0.0f;
    float prev_jitter_x = 0.0f;
    float prev_jitter_y = 0.0f;
    
    // Scale factors
    float scale_x = 2.5f;
    float scale_y = 2.5f;
    float inv_scale_x = 0.4f;
    float inv_scale_y = 0.4f;
    
    // Texture sizes
    float texel_size_x = 1.0f / 512.0f;
    float texel_size_y = 1.0f / 288.0f;
    
    // FSR 2.0 EASU parameters
    float sharpness = 0.5f;
    float edge_threshold = 0.05f;
    int lanczos_lobes = 2;
    
    // TAA parameters
    float temporal_alpha = 0.9f;
    float motion_vector_scale_x = 1.0f;
    float motion_vector_scale_y = 1.0f;
    float disocclusion_threshold = 0.1f;
    float motion_threshold = 0.5f;
    float color_threshold = 0.1f;
    float history_weight = 0.9f;
    
    // Jitter
    float jitter_x = 0.0f;
    float jitter_y = 0.0f;
    float prev_jitter_x = 0.0f;
    float prev_jitter_y = 0.0f;
    
    // Frame index for jitter sequence
    uint32_t frame_index = 0;
    
    // Mode: 0 = FSR2 full, 1 = EASU only, 2 = RCAS only, 3 = TAA only
    int mode = 0;
    
    // Padding for alignment
    float pad0 = 0.0f;
    float pad1 = 0.0f;
};

// FSR 2.x Compute Shader class
class Fsr2Compute {
public:
    Fsr2Compute() = default;
    ~Fsr2Compute() { shutdown(); }

    bool init(VulkanContext* ctx, FramebufferManager* fb_mgr);
    void shutdown();

    // Execute FSR 2.x pipeline: rough (288p) -> EASU -> RCAS -> TAA -> final (720p)
    bool execute(const FramebufferManager::FrameHistory* rough_history,
                 const FramebufferManager::FrameHistory* prev_history,
                 VkCommandBuffer cmd);

    void setSharpness(float sharpness) { sharpness_ = std::clamp(sharpness, 0.0f, 1.0f); }
    void setJitter(float x, float y) { jitter_x_ = x; jitter_y_ = y; }
    void setMode(int mode) { mode_ = mode; } // 0=full, 1=EASU only, 2=RCAS only, 3=TAA only

private:
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
    
    bool use_fsr2_ = true;
    float sharpness_ = 0.5f;
    float jitter_x_ = 0.0f, jitter_y_ = 0.0f;
    int mode_ = 0; // 0=full, 1=EASU only, 2=RCAS only, 3=TAA only
    
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