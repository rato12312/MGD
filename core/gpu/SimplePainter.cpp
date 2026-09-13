// SimplePainter.cpp - Original simple painter implementation
#include "Painter.h"
#include "VulkanBackend.h"
#include <iostream>

namespace mgd {
namespace gpu {

class SimplePainter : public Painter {
public:
    SimplePainter() : ctx_(nullptr), fb_mgr_(nullptr) {}
    ~SimplePainter() override { shutdown(); }

    bool init(VulkanContext* ctx, FramebufferManager* fb_mgr) override {
        ctx_ = ctx;
        fb_mgr_ = fb_mgr;
        std::cout << "[SimplePainter] Initialized" << std::endl;
        return true;
    }

    void shutdown() override {
        ctx_ = nullptr;
        fb_mgr_ = nullptr;
    }

    bool execute(const FramebufferManager::FrameHistory* rough_hist,
                 const FramebufferManager::FrameHistory* prev_hist,
                 VkCommandBuffer cmd) override {
        if (!rough_hist || !rough_hist->valid) return false;
        
        // Simple blit from rough to final - no upscaling, no TAA, no RCAS
        // Just present the rough render as-is
        stats_.frames_rendered++;
        return true;
    }

    void setConfig(const PainterConfig& config) override {
        config_ = config;
    }
    
    const PainterConfig& getConfig() const override {
        return config_;
    }
    
    void setResolution(uint32_t rough_w, uint32_t rough_h, 
                       uint32_t final_w, uint32_t final_h) override {
        rough_w_ = rough_w;
        rough_h_ = rough_h;
        final_w_ = final_w;
        final_h_ = final_h;
    }

    void setConfig(const PainterConfig& config) override {
        config_ = config;
        fsr_enabled_ = config.enable_fsr2;
        taa_enabled_ = config.enable_taa;
        rcas_enabled_ = config.enable_rcas;
        sharpness_ = config.sharpness;
        resolution_scale_ = config.resolution_scale;
        debug_overlay_ = config.debug_overlay;
    }
    
    const PainterConfig& getConfig() const override {
        return config_;
    }
    
    void setResolution(uint32_t rough_w, uint32_t rough_h, 
                       uint32_t final_w, uint32_t final_h) override {
        rough_w_ = rough_w;
        rough_h_ = rough_h;
        final_w_ = final_w;
        final_h_ = final_h;
    }
    
    void setFSREnabled(bool enabled) override {
        fsr_enabled_ = enabled;
    }
    bool isFSREnabled() const override { return fsr_enabled_; }
    
    virtual void setTAAEnabled(bool enabled) {
        taa_enabled_ = enabled;
    }
    bool isTAAEnabled() const { return taa_enabled_; }
    
    virtual void setRCASEnabled(bool enabled) {
        rcas_enabled_ = enabled;
    }
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    void setSharpness(float sharpness) override {
        sharpness_ = std::clamp(sharpness, 0.0f, 1.0f);
    }
    float getSharpness() const override { return sharpness_; }
    
    void setResolutionScale(float scale) override {
        resolution_scale_ = std::clamp(scale, 0.25f, 1.0f);
    }
    float getResolutionScale() const { return resolution_scale_; }
    
    void setFSREnabled(bool enabled) {
        fsr_enabled_ = enabled;
    }
    bool isFSREnabled() const { return fsr_enabled_; }
    
    void setTAAEnabled(bool enabled) override {
        taa_enabled_ = enabled;
    }
    bool isTAAEnabled() const { return taa_enabled_; }
    
    void setRCASEnabled(bool enabled) override {
        rcas_enabled_ = enabled;
    }
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    void setSharpness(float sharpness) override {
        sharpness_ = std::clamp(sharpness, 0.0f, 1.0f);
    }
    float getSharpness() const override { return sharpness_; }
    
    void setResolutionScale(float scale) override {
        resolution_scale_ = std::clamp(scale, 0.25f, 1.0f);
    }
    float getResolutionScale() const { return resolution_scale_; }
    
    void setFSREnabled(bool enabled) override {
        fsr_enabled_ = enabled;
    }
    bool isFSREnabled() const { return fsr_enabled_; }
    
    void setTAAEnabled(bool enabled) override {
        taa_enabled_ = enabled;
    }
    bool isTAAEnabled() const { return taa_enabled_; }
    
    void setRCASEnabled(bool enabled) override {
        rcas_enabled_ = enabled;
    }
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    void setSharpness(float sharpness) override {
        sharpness_ = std::clamp(sharpness, 0.0f, 1.0f);
    }
    float getSharpness() const override { return sharpness_; }
    
    void setResolutionScale(float scale) override {
        resolution_scale_ = std::clamp(scale, 0.25f, 1.0f);
    }
    float getResolutionScale() const { return resolution_scale_; }
    
    void setFSREnabled(bool enabled) {
        fsr_enabled_ = enabled;
    }
    bool isFSREnabled() const { return fsr_enabled_; }
    
    void setTAAEnabled(bool enabled) {
        taa_enabled_ = enabled;
    }
    bool isTAAEnabled() const { return taa_enabled_; }
    
    void setRCASEnabled(bool enabled) override {
        rcas_enabled_ = enabled;
    }
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    void setSharpness(float sharpness) override {
        sharpness_ = std::clamp(sharpness, 0.0f, 1.0f);
    }
    float getSharpness() const { return sharpness_; }
    
    void setResolutionScale(float scale) override {
        resolution_scale_ = std::clamp(scale, 0.25f, 1.0f);
    }
    float getResolutionScale() const { return resolution_scale_; }
    
    void setFSREnabled(bool enabled) override {
        fsr_enabled_ = enabled;
    }
    bool isFSREnabled() const { return fsr_enabled_; }
    
    void setTAAEnabled(bool enabled) override {
        taa_enabled_ = enabled;
    }
    bool isTAAEnabled() const { return taa_enabled_; }
    
    void setRCASEnabled(bool enabled) override {
        rcas_enabled_ = enabled;
    }
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    void setDebugOverlay(bool enabled) override {
        debug_overlay_ = enabled;
    }
    bool isDebugOverlayEnabled() const { return debug_overlay_; }
    
    void setDebugOverlay(bool enabled) override {
        debug_overlay_ = enabled;
    }
    bool isDebugOverlayEnabled() const { return debug_overlay_; }
    
    const Stats& getStats() const override {
        return stats_;
    }
    
    void setFSREnabled(bool enabled) override {
        fsr_enabled_ = enabled;
    }
    bool isFSREnabled() const { return fsr_enabled_; }
    
    void setTAAEnabled(bool enabled) override {
        taa_enabled_ = enabled;
    }
    bool isTAAEnabled() const { return taa_enabled_; }
    
    void setRCASEnabled(bool enabled) override {
        rcas_enabled_ = enabled;
    }
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    void setSharpness(float sharpness) override {
        sharpness_ = std::clamp(sharpness, 0.0f, 1.0f);
    }
    float getSharpness() const { return sharpness_; }
    
    void setResolutionScale(float scale) override {
        resolution_scale_ = std::clamp(scale, 0.25f, 1.0f);
    }
    float getResolutionScale() const { return resolution_scale_; }
    
    void setFSREnabled(bool enabled) override {
        fsr_enabled_ = enabled;
    }
    bool isFSREnabled() const { return fsr_enabled_; }
    
    void setTAAEnabled(bool enabled) override {
        taa_enabled_ = enabled;
    }
    bool isTAAEnabled() const { return taa_enabled_; }
    
    void setRCASEnabled(bool enabled) override {
        rcas_enabled_ = enabled;
    }
    bool isRCASEnabled() const { return rcas_enabled_; }
    
    void setDebugOverlay(bool enabled) override {
        debug_overlay_ = enabled;
    }
    bool isDebugOverlayEnabled() const { return debug_overlay_; }
    
    const Stats& getStats() const override {
        return stats_;
    }
    
    void setDebugOverlay(bool enabled) override {
        debug_overlay_ = enabled;
    }
    bool isDebugOverlayEnabled() const { return debug_overlay_; }
    
    const Stats& getStats() const override {
        return stats_;
    }
    
    // Configuration
    void setConfig(const PainterConfig& config) override {
        config_ = config;
        fsr_enabled_ = config.enable_fsr2;
        taa_enabled_ = config.enable_taa;
        rcas_enabled_ = config.enable_rcas;
        sharpness_ = config.sharpness;
        resolution_scale_ = config.resolution_scale;
        debug_overlay_ = config.debug_overlay;
    }
    
    const PainterConfig& getConfig() const override {
        return config_;
    }

private:
    VulkanContext* ctx_ = nullptr;
    FramebufferManager* fb_mgr_ = nullptr;
    PainterConfig config_;
    uint32_t rough_w_ = 512, rough_h_ = 288;
    uint32_t final_w_ = 1280, final_h_ = 720;
    
    PainterConfig config_;
    bool fsr_enabled_ = true;
    bool taa_enabled_ = true;
    bool rcas_enabled_ = true;
    float sharpness_ = 0.5f;
    float resolution_scale_ = 0.67f;
    bool debug_overlay_ = false;
    
    Stats stats_;
    float sharpness_ = 0.5f;
    float resolution_scale_ = 0.67f;
    bool fsr_enabled_ = true;
    bool taa_enabled_ = true;
    bool rcas_enabled_ = true;
    bool debug_overlay_ = false;
    Stats stats_;
};

} // namespace gpu
} // namespace mgd