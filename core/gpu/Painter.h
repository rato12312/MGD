// Painter.h - Abstract Painter Interface
// Allows switching between different painter implementations

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>

#include "VulkanContext.h"
#include "FramebufferManager.h"

namespace mgd {
namespace gpu {

// Frame data for painter
struct PainterFrameData {
    VkImageView rough_color = VK_NULL_HANDLE;
    VkImageView rough_depth = VK_NULL_HANDLE;
    VkImageView prev_color = VK_NULL_HANDLE;
    VkImageView prev_depth = VK_NULL_HANDLE;
    VkImageView motion_vectors = VK_NULL_HANDLE;
    uint32_t frame_index = 0;
    float delta_time = 0.0f;
};

// Painter configuration
struct PainterConfig {
    enum class Type {
        SIMPLE,           // Original simple painter
        INTELLIGENT,      // New intelligent painter with FSR2/TAA/RCAS/MFO
        LEGACY            // Legacy compatibility
    };
    
    Type type = Type::INTELLIGENT;
    
    // Intelligent painter options
    bool enable_fsr2 = true;
    bool enable_taa = true;
    bool enable_rcas = true;
    bool enable_mfo = true;
    bool enable_seed_predictor = true;
    float sharpness = 0.5f;
    float resolution_scale = 0.67f;
    float lod_bias = 0.0f;
    int target_fps = 60;
    bool vsync = true;
    bool debug_overlay = false;
};

// Abstract Painter Interface
class Painter {
public:
    virtual ~Painter() = default;
    
    // Initialize with Vulkan context and framebuffer manager
    virtual bool init(VulkanContext* ctx, FramebufferManager* fb_mgr) = 0;
    virtual void shutdown() = 0;
    
    // Main render function
    // rough_hist: current frame rough render
    // prev_hist: previous frame history (for TAA)
    // cmd: command buffer to record commands into
    virtual bool execute(const FramebufferManager::FrameHistory* rough_hist,
                         const FramebufferManager::FrameHistory* prev_hist,
                         VkCommandBuffer cmd) = 0;
    
    // Configuration
    virtual void setConfig(const PainterConfig& config) = 0;
    virtual const PainterConfig& getConfig() const = 0;
    
    // Resolution handling
    virtual void setResolution(uint32_t rough_w, uint32_t rough_h, 
                               uint32_t final_w, uint32_t final_h) = 0;
    
    // Quality control
    virtual void setFSREnabled(bool enabled) = 0;
    virtual void setTAAEnabled(bool enabled) = 0;
    virtual void setRCASEnabled(bool enabled) = 0;
    virtual void setSharpness(float sharpness) = 0;
    
    // FSR 2.x specific
    virtual void setFSRMode(int mode) = 0; // 0=off, 1=quality, 2=balanced, 3=performance, 4=ultra_perf
    
    // Sharpness
    virtual void setSharpness(float sharpness) = 0;
    float getSharpness() const { return sharpness_; }
    
    // Resolution scale
    virtual void setResolutionScale(float scale) = 0;
    float getResolutionScale() const { return resolution_scale_; }
    
    // FSR/TAA/RCAS toggles
    virtual void setFSREnabled(bool enabled) = 0;
    virtual bool isFSREnabled() const { return fsr_enabled_; }
    
    virtual void setTAAEnabled(bool enabled) = 0;
    bool isTAAEnabled() const { return taa_enabled_; }
    
    virtual void setRCASEnabled(bool enabled) = 0;
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    // Debug
    virtual void setDebugOverlay(bool enabled) = 0;
    bool isDebugOverlayEnabled() const { return debug_overlay_; }
    
    // Stats
    struct Stats {
        uint64_t frames_rendered = 0;
        double avg_frame_time_ms = 0.0;
        double avg_upscale_time_ms = 0.0;
        uint64_t fsr2_frames = 0;
        uint64_t taa_frames = 0;
        uint64_t rcas_frames = 0;
        float mfo_reuse_ratio = 0.0f;
    };
    virtual const Stats& getStats() const = 0;
    
protected:
    float sharpness_ = 0.5f;
    float resolution_scale_ = 0.67f;
    bool fsr_enabled_ = true;
    bool taa_enabled_ = true;
    bool rcas_enabled_ = true;
    bool debug_overlay_ = false;
};

} // namespace gpu
} // namespace mgd