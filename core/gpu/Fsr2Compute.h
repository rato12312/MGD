#pragma once

// FSR 2.x Compute Shader Pipeline
// Complete FSR 2.x implementation: EASU + RCAS + TAA
// Based on AMD FidelityFX FSR 2.x reference implementation

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>

#include "VulkanContext.h"
#include "FramebufferManager.h"
#include "Fsr2EasuCompute.h"
#include "Fsr2Rcas.h"
#include "Fsr2Taa.h"

namespace mgd {
namespace gpu {

// FSR 2.x Quality Presets
enum class Fsr2Quality {
    Ultra,      // Native 4K -> 4K (no upscale), just RCAS
    Quality,    // 1440p -> 4K (1.5x scale) - Quality mode
    Balanced,   // 1080p -> 4K (1.7x scale) - Balanced mode  
    Performance, // 720p -> 4K (2x scale) - Performance mode
    UltraPerformance // 540p -> 4K (3x scale) - Ultra Performance
};

// FSR 2.x configuration
struct Fsr2Config {
    Fsr2Quality quality = Fsr2Quality::Balanced;
    bool enable_taa = true;
    bool enable_rcas = true;
    float sharpness = 0.5f;
    
    // Custom resolution scale (overrides quality preset)
    float custom_scale = 0.0f; // 0 = use preset
    
    // TAA settings
    bool enable_taa = true;
    float temporal_alpha = 0.9f;
    float disocclusion_threshold = 0.1f;
    float motion_threshold = 0.5f;
    float color_threshold = 0.1f;
};

// FSR 2.x pipeline state
struct Fsr2State {
    // Input resources (from rascunho / game)
    VkImageView input_color;      // Rascunho color (rough resolution)
    VkImageView input_depth;      // Rascunho depth
    VkImageView input_motion;     // Motion vectors (RG16F)
    VkImageView input_obj_id;     // Object IDs
    
    // History resources (previous frame)
    VkImageView prev_color;       // Previous frame color (EASU output)
    VkImageView prev_depth;       // Previous depth
    VkImageView prev_motion;      // Previous motion vectors
    VkImageView prev_obj_id;      // Previous object IDs
    
    // Output
    VkImageView output_color;     // Final 720p/1080p/4K output
    VkImageView output_depth;     // Output depth
    
    // Internal intermediate resources
    VkImageView easu_output;      // EASU output
    VkImageView easu_depth;       // EASU depth
    VkImageView rcas_output;      // RCAS output
    
    // History resources (ping-pong)
    VkImageView history_color[2];   // Ping-pong history
    VkImageView history_depth[2];
    VkImageView history_motion[2];
    VkImageView history_obj_id[2];
    int history_index = 0;
    
    // Motion vectors
    VkImageView motion_vectors;   // Current frame motion vectors
    VkImageView prev_motion;      // Previous frame motion vectors
    
    // Frame info
    uint64_t frame_index = 0;
    uint32_t input_width = 0;
    uint32_t input_height = 0;
    uint32_t output_width = 0;
    uint32_t output_height = 0;
};

// FSR 2.x Quality Presets
struct Fsr2QualityPreset {
    float scale_factor;      // Input -> Output scale
    const char* name;
    const char* description;
};

inline const Fsr2QualityPreset FSR2_QUALITY_PRESETS[] = {
    { 1.0f, "Ultra", "Native resolution, RCAS only" },
    { 1.5f, "Quality", "1440p -> 4K (1.5x)" },
    { 1.7f, "Balanced", "1080p -> 4K (1.7x)" },
    { 2.0f, "Performance", "720p -> 4K (2x)" },
    { 3.0f, "Ultra Performance", "540p -> 4K (3x)" }
};

// FSR 2.x Pipeline State
class Fsr2Pipeline {
public:
    Fsr2Pipeline() = default;
    ~Fsr2Pipeline() { shutdown(); }
    
    bool init(VulkanContext* ctx, FramebufferManager* fb_mgr, const Fsr2Config& config);
    void shutdown();
    
    // Main execution: rough frame -> final output
    // Input: rough_color, rough_depth, motion_vectors, obj_id
    // Output: final_color (final resolution)
    bool execute(VkCommandBuffer cmd,
                 VkImageView rough_color,
                 VkImageView rough_depth,
                 VkImageView motion_vectors,
                 VkImageView obj_id,
                 VkImageView output_color,
                 VkImageView output_depth,
                 uint64_t frame_index);
    
    void setQuality(Fsr2Quality quality) { config_.quality = quality; }
    void setSharpness(float sharpness) { config_.sharpness = sharpness; }
    void setTAAEnabled(bool enable) { config_.enable_taa = enable; }
    void setRCASEnabled(bool enable) { config_.enable_rcas = enable; }
    void setCustomScale(float scale) { config_.custom_scale = scale; }
    
    const Fsr2Config& getConfig() const { return config_; }
    uint32_t getInputWidth() const { return input_width_; }
    uint32_t getInputHeight() const { return input_height_; }
    uint32_t getOutputWidth() const { return output_width_; }
    uint32_t getOutputHeight() const { return output_height_; }
    
    // Stats
    uint64_t getFrameIndex() const { return frame_index_; }
    double getLastEASUTimeMs() const { return last_easu_time_ms_; }
    double getLastRCASTimeMs() const { return last_rcas_time_ms_; }
    double getLastTAATimeMs() const { return last_taa_time_ms_; }

private:
    bool createResources(VulkanContext* ctx, FramebufferManager* fb_mgr);
    void destroyResources();
    bool createPipelines(VulkanContext* ctx);
    bool createResources(VulkanContext* ctx, FramebufferManager* fb_mgr);
    void updateHistory(uint64_t frame_index);
    void executeEASU(VkCommandBuffer cmd);
    void executeRCAS(VkCommandBuffer cmd);
    void executeTAA(VkCommandBuffer cmd, uint64_t frame_index);
    void swapHistory();
    
    // Shaders
    bool createShaders(VulkanContext* ctx);
    bool createPipelines(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    void updateDescriptorSets(uint64_t frame_index);
    void updatePushConstants(uint64_t frame_index);
    
    // EASU pass
    void recordEASU(VkCommandBuffer cmd);
    
    // RCAS pass
    void recordRCAS(VkCommandBuffer cmd);
    
    // TAA pass
    void recordTAA(VkCommandBuffer cmd, uint64_t frame_index);
    
    // History management
    void rotateHistory();
    void clearHistory();
    
    // Jitter sequence (Halton base 2,3)
    void updateJitter(uint64_t frame_index);
    
    // FSR 2.x config
    Fsr2Config config_;
    
    // Dimensions
    uint32_t input_width_ = 0;
    uint32_t input_height_ = 0;
    uint32_t output_width_ = 0;
    uint32_t output_height_ = 0;
    
    // Vulkan resources
    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    
    // EASU
    std::unique_ptr<Fsr2EasuCompute> easu_;
    
    // RCAS
    std::unique_ptr<Fsr2Rcas> rcas_;
    
    // TAA
    std::unique_ptr<Fsr2Taa> taa_;
    
    // Resources
    struct ImageResource {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width = 0;
        uint32_t height = 0;
        VkImageUsageFlags usage = 0;
    };
    
    // History buffers (ping-pong)
    struct HistoryBuffer {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    
    HistoryBuffer history_color_[2];
    HistoryBuffer history_depth_[2];
    HistoryBuffer history_motion_[2];
    HistoryBuffer history_obj_id_[2];
    int history_index_ = 0;
    
    // Current frame resources
    VkImageView current_rough_color_;
    VkImageView current_rough_depth_;
    VkImageView current_motion_;
    VkImageView current_obj_id_;
    
    // Output
    VkImageView output_color_;
    VkImageView output_depth_;
    
    // Intermediate
    VkImageView easu_output_;
    VkImageView easu_depth_;
    VkImageView rcas_output_;
    
    // Motion vectors
    VkImageView current_motion_;
    VkImageView prev_motion_;
    
    // Frame info
    uint64_t frame_index_ = 0;
    uint32_t input_width_ = 0;
    uint32_t input_height_ = 0;
    uint32_t output_width_ = 0;
    uint32_t output_height_ = 0;
    
    // Timing
    double last_easu_time_ms_ = 0;
    double last_rcas_time_ms_ = 0;
    double last_taa_time_ms_ = 0;
    
    // Jitter sequence (Halton base 2,3)
    float jitter_x_ = 0.0f;
    float jitter_y_ = 0.0f;
    float prev_jitter_x_ = 0.0f;
    float prev_jitter_y_ = 0.0f;
    
    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    
    // FSR 2.x components
    std::unique_ptr<Fsr2EasuCompute> easu_;
    std::unique_ptr<Fsr2Rcas> rcas_;
    std::unique_ptr<Fsr2Taa> taa_;
    
    // Vulkan resources
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule easu_module_ = VK_NULL_HANDLE;
    VkShaderModule rcas_module_ = VK_NULL_HANDLE;
    VkShaderModule taa_module_ = VK_NULL_HANDLE;
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
    VkBuffer push_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory push_memory_ = VK_NULL_HANDLE;
    
    bool createResources(VulkanContext* ctx, FramebufferManager* fb_mgr);
    void destroyResources();
    bool createPipelines(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    void updateHistory(uint64_t frame_index);
    void executeEASU(VkCommandBuffer cmd);
    void executeRCAS(VkCommandBuffer cmd);
    void executeTAA(VkCommandBuffer cmd, uint64_t frame_index);
    void swapHistory();
    void updateJitter(uint64_t frame_index);
};

} // namespace gpu
} // namespace mgd