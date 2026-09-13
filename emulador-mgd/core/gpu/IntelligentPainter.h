#pragma once

// Intelligent Painter for Mario Odyssey
// Combines framebuffer analysis, MFO optimization, and intelligent upscaling
// Specifically designed for Super Mario Odyssey's framebuffer characteristics

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <array>
#include <cstdint>
#include <functional>

#include "VulkanContext.h"
#include "FramebufferManager.h"
#include "FramebufferOptimizer.h"
#include "SeedPredictor.h"
#include "ShaderRecompiler.h"

namespace mgd {
namespace gpu {

// Mario Odyssey specific framebuffer layout
struct OdysseyFramebufferLayout {
    // Main framebuffer (720p target)
    VkExtent2D target_extent = {1280, 720};
    VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    
    // Rascunho (internal render resolution - e.g., 0.5x = 640x360)
    VkExtent2D rough_extent = {640, 360};
    float resolution_scale = 0.5f;
    
    // Multi-sample count
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    
    // Layer count for stereo/multi-view
    uint32_t layer_count = 1;
};

// Odyssey-specific render passes
enum class OdysseyRenderPassType {
    MAIN_SCENE,           // Main 3D scene
    UI_OVERLAY,           // HUD, menus
    POST_PROCESS,         // Bloom, tone mapping
    MOTION_VECTORS,       // For TAA/FSR2
    DEPTH_PREPASS,        // Depth pre-pass
    SHADOW_MAP,           // Shadow rendering
    REFLECTION_PROBE,     // Reflection probes
    PARTICLES,            // Particle systems
    TRANSPARENT,          // Transparent objects
    SKYBOX,               // Skybox/background
    DEBUG_OVERLAY         // Debug visualization
};

// Intelligent upscaling configuration
struct IntelligentUpscaleConfig {
    enum class Mode {
        NONE,           // Native resolution
        FSR2_QUALITY,   // FSR 2.x Quality mode
        FSR2_BALANCED,  // FSR 2.x Balanced mode
        FSR2_PERFORMANCE, // FSR 2.x Performance mode
        FSR2_ULTRA_PERFORMANCE, // FSR 2.x Ultra Performance
        CUSTOM          // Custom shader
    };
    
    Mode mode = Mode::FSR2_BALANCED;
    bool enable_taa = true;
    bool enable_rcas = true;
    float sharpness = 0.5f;
    float temporal_stability = 0.9f;
    float motion_threshold = 0.5f;
    bool enable_motion_vectors = true;
    bool enable_depth_rejection = true;
    bool enable_disocclusion_handling = true;
};

// Mario Odyssey specific game state for intelligent rendering
struct OdysseyGameState {
    // Camera state
    struct Camera {
        float position[3] = {0, 0, 0};
        float rotation[4] = {0, 0, 0, 1}; // quaternion
        float fov = 60.0f;
        float aspect = 16.0f / 9.0f;
        float near_plane = 0.1f;
        float far_plane = 1000.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
    } camera;
    
    // Mario state
    struct Mario {
        float position[3] = {0, 0, 0};
        float velocity[3] = {0, 0, 0};
        uint32_t state = 0; // animation state
        uint32_t power_up = 0; // cap, etc.
        bool is_capturing = false;
        uint64_t capture_target = 0;
    } mario;
    
    // Current kingdom/level
    uint32_t current_kingdom = 0;
    uint32_t current_area = 0;
    
    // Camera mode
    enum class CameraMode {
        FOLLOW,
        FIXED,
        FIRST_PERSON,
        CAPTURE,
        CINEMATIC
    } camera_mode = CameraMode::FOLLOW;
    
    // Frame info
    uint64_t frame_index = 0;
    float delta_time = 0.0f;
    
    // Visual settings
    bool is_2d_section = false;
    bool is_capture_sequence = false;
    bool is_loading = false;
};

// Intelligent render decision
struct RenderDecision {
    OdysseyRenderPassType pass_type;
    VkExtent2D render_extent;
    bool use_mfo = true;
    bool use_fsr = false;
    bool use_taa = false;
    bool use_rcas = false;
    bool skip_render = false;
    float priority = 1.0f; // 0.0 - 1.0
    std::string reason;
};

// Intelligent Painter - the main class
class IntelligentPainter {
public:
    IntelligentPainter() = default;
    ~IntelligentPainter() = default;
    
    // Initialize with Vulkan context and MFO
    bool init(VulkanContext* ctx, FramebufferOptimizer* mfo, 
              SeedPredictor* seed_predictor = nullptr);
    void shutdown();
    
    // Configure for Mario Odyssey
    void configureForOdyssey(const OdysseyFramebufferLayout& layout);
    
    // Set game state for intelligent decisions
    void updateGameState(const OdysseyGameState& state);
    
    // Configure upscaling
    void setUpscaleConfig(const IntelligentUpscaleConfig& config);
    
    // Main render function - intelligently decides how to render
    bool renderFrame(const uint8_t* cmd_buffer, size_t size, 
                     const OdysseyGameState& game_state,
                     VkCommandBuffer cmd_buffer_vk);
    
    // Intelligent render decision making
    RenderDecision makeRenderDecision(const OdysseyGameState& state, 
                                      OdysseyRenderPassType pass_type);
    
    // Execute specific render pass intelligently
    bool executePass(VkCommandBuffer cmd, const RenderDecision& decision,
                     const OdysseyGameState& state);
    
    // FSR 2.x upscale
    bool executeFSR2Upscale(VkCommandBuffer cmd, VkImageView input,
                            VkImageView output, VkImageView motion_vectors,
                            VkImageView depth, VkImageView history);
    
    // TAA pass
    bool executeTAA(VkCommandBuffer cmd, VkImageView current, VkImageView history,
                    VkImageView motion_vectors, VkImageView depth);
    
    // RCAS sharpen
    bool executeRCAS(VkCommandBuffer cmd, VkImageView input, VkImageView output);
    
    // Motion vector generation
    bool generateMotionVectors(VkCommandBuffer cmd, 
                               const OdysseyGameState& prev_state,
                               const OdysseyGameState& curr_state,
                               VkImageView output);
    
    // MFO integration
    void processMFO(VkCommandBuffer cmd, const OdysseyGameState& state);
    
    // Statistics
    struct Stats {
        uint64_t frames_rendered = 0;
        uint64_t frames_skipped = 0;
        uint64_t fsr2_frames = 0;
        uint64_t taa_frames = 0;
        uint64_t rcas_frames = 0;
        uint64_t mfo_tiles_reused = 0;
        uint64_t mfo_tiles_rebuilt = 0;
        double avg_frame_time_ms = 0.0;
        double avg_upscale_time_ms = 0.0;
        double mfo_reuse_ratio = 0.0;
    };
    const Stats& getStats() const { return stats_; }
    
    // Get current upscale mode
    IntelligentUpscaleConfig::Mode getUpscaleMode() const { return upscale_config_.mode; }
    
    // Dynamic quality adjustment
    void adjustQualityForPerformance(float target_fps);
    
private:
    // Core components
    VulkanContext* ctx_ = nullptr;
    FramebufferOptimizer* mfo_ = nullptr;
    SeedPredictor* seed_predictor_ = nullptr;
    
    // Configuration
    OdysseyFramebufferLayout layout_;
    IntelligentUpscaleConfig upscale_config_;
    OdysseyGameState current_state_;
    OdysseyGameState prev_state_;
    
    // Pipelines
    struct PipelineSet {
        VkPipeline fsr2_easu = VK_NULL_HANDLE;
        VkPipeline fsr2_rcas = VK_NULL_HANDLE;
        VkPipeline taa = VK_NULL_HANDLE;
        VkPipeline rcas = VK_NULL_HANDLE;
        VkPipeline motion_vectors = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
    };
    std::unordered_map<std::string, PipelineSet> pipelines_;
    
    // Descriptor sets
    VkDescriptorSetLayout fsr2_desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool fsr2_desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> fsr2_desc_sets_;
    
    // Resources
    VkImageView history_color_ = VK_NULL_HANDLE;
    VkImageView history_depth_ = VK_NULL_HANDLE;
    VkImageView motion_vectors_ = VK_NULL_HANDLE;
    VkBuffer frame_data_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory frame_data_memory_ = VK_NULL_HANDLE;
    
    // Previous frame state for TAA/motion vectors
    OdysseyGameState prev_state_;
    bool has_prev_state_ = false;
    
    // Statistics
    Stats stats_;
    
    // Configuration
    IntelligentUpscaleConfig upscale_config_;
    OdysseyFramebufferLayout layout_;
    OdysseyGameState current_state_;
    
    // Private methods
    bool createPipelines();
    bool createDescriptorSets();
    bool createFrameResources();
    void destroyPipelines();
    void destroyFrameResources();
    
    // Decision making
    bool shouldUseFSR2(const OdysseyGameState& state, OdysseyRenderPassType pass);
    bool shouldUseTAA(const OdysseyGameState& state, OdysseyRenderPassType pass);
    bool shouldUseRCAS(const OdysseyGameState& state);
    bool shouldSkipFrame(const OdysseyGameState& state, OdysseyRenderPassType pass);
    float calculateLOD(const OdysseyGameState& state, float distance);
    
    // FSR2 implementation
    bool setupFSR2Constants(VkCommandBuffer cmd, const IntelligentUpscaleConfig& config);
    bool dispatchFSR2EASU(VkCommandBuffer cmd, VkImageView input, VkImageView output);
    bool dispatchFSR2RCAS(VkCommandBuffer cmd, VkImageView input, VkImageView output);
    
    // TAA implementation
    bool setupTAAConstants(VkCommandBuffer cmd, const OdysseyGameState& prev, 
                           const OdysseyGameState& curr);
    bool dispatchTAA(VkCommandBuffer cmd, VkImageView current, VkImageView history,
                     VkImageView motion, VkImageView depth);
    
    // RCAS implementation
    bool dispatchRCAS(VkCommandBuffer cmd, VkImageView input, VkImageView output);
    
    // Motion vectors
    bool generateMotionVectorsImpl(VkCommandBuffer cmd,
                                   const OdysseyGameState& prev,
                                   const OdysseyGameState& curr,
                                   VkImageView output);
    
    // MFO integration
    void updateMFOTiles(const OdysseyGameState& state);
    void submitMFOTiles(VkCommandBuffer cmd);
    
    // Quality adjustment
    void analyzePerformance();
    void adjustUpscaleMode();
    
    // Pipeline creation helpers
    VkPipeline createGraphicsPipeline(const std::string& name,
                                       const std::vector<uint32_t>& vert_spirv,
                                       const std::vector<uint32_t>& frag_spirv,
                                       VkRenderPass render_pass);
    VkPipeline createComputePipeline(const std::string& name,
                                      const std::vector<uint32_t>& comp_spirv);
    
    // Shader loading
    std::vector<uint32_t> loadShader(const std::string& path);
    std::vector<uint32_t> compileShaderGLSL(const std::string& source, 
                                             VkShaderStageFlagBits stage);
    
    // Framebuffer layout analysis
    bool analyzeFramebufferLayout(const uint8_t* cmd_buffer, size_t size);
    
    // Stats
    Stats stats_;
    uint64_t frame_count_ = 0;
    double frame_time_accumulator_ = 0.0;
    double last_quality_adjustment_ = 0.0;
    
    // Quality targets
    float target_fps_ = 60.0f;
    float min_acceptable_fps_ = 30.0f;
    float quality_adjustment_threshold_ = 0.1f;
};

} // namespace gpu
} // namespace mgd