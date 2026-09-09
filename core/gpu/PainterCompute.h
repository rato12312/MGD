#pragma once

// Painter Compute Shader — reconstrução temporal 720p a partir de rascunho 288p
// Recebe: rough_color (288p), rough_depth (288p), rough_obj_id (288p)
//        prev_frame (720p), motion_vectors (288p), prev_obj_id (288p)
// Saída: final_720p RGBA8

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <array>
#include <cstdint>

#include "core/gpu/FramebufferManager.h"
#include "core/gpu/Fsr10.h"

namespace mgd {
namespace gpu {

// Push constants para Painter compute
struct PainterPushConstants {
    // Rough frame (288p)
    int rough_w = 512, rough_h = 288;
    // Final frame (720p)
    int final_w = 1280, final_h = 720;
    // Scale factor
    float scale_x = 2.5f, scale_y = 2.5f;
    // Jitter offset for TAA
    float jitter_x = 0.0f, jitter_y = 0.0f;
    // Temporal stability
    float temporal_alpha = 0.9f;    // blend com frame anterior
    float edge_threshold = 0.05f;   // detecção de borda no depth
    float detail_strength = 0.5f;   // síntese de detalhe
    uint32_t frame_index = 0;
    int mode = 0; // 0=FSR2+TAA, 1=FSR1, 2=edge recon, 3=detail synth
};

// FSR 2.x constants
struct Fsr2Constants {
    // EASU constants
    float scale_x = 2.5f;
    float scale_y = 2.5f;
    float inv_scale_x = 0.4f;
    float inv_scale_y = 0.4f;
    float texel_size_x = 1.0f / 512.0f;
    float texel_size_y = 1.0f / 288.0f;
    float jitter_x = 0.0f;
    float jitter_y = 0.0f;
    float sharpness = 0.5f;
    float edge_threshold = 0.05f;
    
    // TAA constants
    float temporal_alpha = 0.9f;
    float motion_vector_scale_x = 1.0f;
    float motion_vector_scale_y = 1.0f;
    float disocclusion_threshold = 0.1f;
    float motion_threshold = 0.5f;
};

// Painter compute pipeline
class PainterCompute {
public:
    PainterCompute() = default;
    ~PainterCompute() { shutdown(); }

    bool init(VulkanContext* ctx, FramebufferManager* fb_mgr);
    void shutdown();

    // Executa compute shader: rough (288p) + history -> final (720p)
    // Inputs: rough_color, rough_depth, rough_obj_id (do rascunho)
    // Output: final_image (720p RGBA8)
    bool execute(const FramebufferManager::FrameHistory* rough_history,
                 const FramebufferManager::FrameHistory* prev_history,
                 VkCommandBuffer cmd);

    // FSR 2.x controls
    void setFSREnabled(bool enabled) { use_fsr2_ = enabled; }
    void setSharpness(float sharpness) { fsr2_const_.sharpness = std::clamp(sharpness, 0.0f, 1.0f); }
    void setJitter(float x, float y) { fsr2_const_.jitter_x = x; fsr2_const_.jitter_y = y; }

    // Pipeline access
    VkPipeline pipeline() const { return pipeline_; }
    VkPipelineLayout layout() const { return layout_; }

private:
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);

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
    
    bool use_fsr2_ = true;
    struct Fsr2Constants {
        // EASU
        float scale_x = 2.5f;
        float scale_y = 2.5f;
        float inv_scale_x = 0.4f;
        float inv_scale_y = 0.4f;
        float texel_size_x = 1.0f / 512.0f;
        float texel_size_y = 1.0f / 288.0f;
        float jitter_x = 0.0f;
        float jitter_y = 0.0f;
        float sharpness = 0.5f;
        float edge_threshold = 0.05f;
        
        // TAA
        float temporal_alpha = 0.9f;
        float motion_vector_scale_x = 1.0f;
        float motion_vector_scale_y = 1.0f;
        float disocclusion_threshold = 0.1f;
        float motion_threshold = 0.5f;
    } fsr2_const_;
    
    bool use_fsr2_ = true;

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
};

} // namespace gpu
} // namespace mgd