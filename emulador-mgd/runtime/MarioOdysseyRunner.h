#pragma once

// MarioOdysseyRunner - Boot real do Super Mario Odyssey com melhores gráficos
// Integra todos os sistemas: Mental Map, Culling, Shaders, FSR, Framebuffer Optimizer

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <functional>

#include "../runtime/Emulator.h"
#include "../gpu/VulkanBackend.h"
#include "../gpu/FramebufferOptimizer.h"
#include "../gpu/PainterCompute.h"
#include "../gpu/Fsr2Compute.h"
#include "../gpu/FrustumCullingCompute.h"
#include "../odyssey/OdysseyWorld.h"
#include "../odyssey/OdysseyHandoff.h"
#include "../loader/Keys.h"
#include "../loader/NspLoader.h"
#include "../loader/NcaSections.h"
#include "../loader/Pfs0.h"
#include "../loader/RomFs.h"
#include "../loader/NsoLoader.h"
#include "../hod/Kernel.h"
#include "../hos/NvService.h"
#include "../core/bridge/MentalMapRuntime.h"
#include "../core/bridge/EmulatorHandoff.h"
#include "../core/query/CameraMentalMapQuery.h"
#include "../core/query/FrustumCullingCompute.h"
#include "../core/query/RegionPolygonCache.h"
#include "../core/query/PolygonConsultant.h"

namespace mgd {
namespace emu {

// Quality Presets para diferentes níveis de hardware
enum class QualityPreset {
    Ultra,          // 720p nativo, sem upscale, shaders completos
    Quality,        // 960x540 -> FSR 2.x Quality -> 720p
    Balanced,       // 854x480 -> FSR 2.x Balanced -> 720p
    Performance,    // 512x288 -> FSR 2.x Performance -> 720p (atual)
    UltraPerformance // 480x270 -> FSR 2.x Ultra Performance -> 720p
};

// Configuração de qualidade gráfica
struct GraphicsQualityConfig {
    QualityPreset preset = QualityPreset::Balanced;
    bool enable_fsr = true;
    bool enable_taa = true;
    bool enable_rcas = true;
    bool enable_frustum_culling = true;
    bool enable_lod = true;
    bool enable_occlusion_culling = true;
    bool enable_frustum_culling = true;
    float sharpness = 0.5f;
    float lod_bias = 0.0f; // -1.0 a 1.0
    int max_polygons_per_frame = 100000;
    float resolution_scale = 1.0f; // 1.0 = nativo, 0.5 = half
    
    static GraphicsQualityConfig fromPreset(QualityPreset preset) {
        GraphicsQualityConfig cfg;
        cfg.preset = preset;
        switch (preset) {
            case QualityPreset::Ultra:
                cfg.resolution_scale = 1.0f;
                cfg.enable_fsr = false;
                cfg.sharpness = 0.0f;
                break;
            case QualityPreset::Quality:
                cfg.resolution_scale = 0.75f; // 960x540 -> 720p
                cfg.enable_fsr = true;
                cfg.sharpness = 0.3f;
                break;
            case QualityPreset::Balanced:
                cfg.resolution_scale = 0.67f; // 854x480 -> 720p
                cfg.enable_fsr = true;
                cfg.sharpness = 0.5f;
                break;
            case QualityPreset::Performance:
                cfg.resolution_scale = 0.5f; // 512x288 -> 720p
                cfg.enable_fsr = true;
                cfg.sharpness = 0.7f;
                break;
            case QualityPreset::UltraPerformance:
                cfg.resolution_scale = 0.375f; // 480x270 -> 720p
                cfg.enable_fsr = true;
                cfg.sharpness = 0.8f;
                break;
        }
        return cfg;
    }
};

// Configuração completa do Mario Odyssey
struct MarioOdysseyConfig {
    std::string nsp_path;
    std::string keys_dir;
    std::string save_dir;
    GraphicsQualityConfig graphics;
    bool auto_load = true;
    bool auto_save = true;
    bool enable_debug_overlay = false;
    bool enable_performance_metrics = true;
    int target_fps = 60;
    bool vsync = true;
    
    static MarioOdysseyConfig defaultConfig() {
        MarioOdysseyConfig cfg;
        cfg.nsp_path = "/sdcard/MGD/game.nsp";
        cfg.keys_dir = "/sdcard/MGD/keys/";
        cfg.save_dir = "/sdcard/MGD/saves/";
        cfg.graphics = GraphicsQualityConfig::fromPreset(QualityPreset::Balanced);
        cfg.auto_load = true;
        cfg.auto_save = true;
        cfg.target_fps = 60;
        cfg.vsync = true;
        return cfg;
    }
};

// Estatísticas de performance em tempo real
struct PerformanceStats {
    uint64_t frame_count = 0;
    double current_fps = 0.0;
    double avg_fps = 0.0;
    double min_fps = 999.0;
    double max_fps = 0.0;
    double frame_time_ms = 0.0;
    double cpu_time_ms = 0.0;
    double gpu_time_ms = 0.0;
    uint64_t triangles_rendered = 0;
    uint32_t draw_calls = 0;
    uint32_t vertices_rendered = 0;
    uint64_t vram_used_mb = 0;
    uint64_t vram_budget_mb = 0;
    float cpu_usage = 0.0f;
    float gpu_usage = 0.0f;
    float battery_level = 1.0f;
    float temperature_c = 0.0f;
    bool thermal_throttling = false;
};

// Callback para atualização de UI/debug
using DebugOverlayCallback = std::function<void(const PerformanceStats&)>;

// Runner principal do Mario Odyssey
class MarioOdysseyRunner {
public:
    MarioOdysseyRunner();
    ~MarioOdysseyRunner();
    
    // Inicialização completa
    bool initialize(const MarioOdysseyConfig& config = MarioOdysseyConfig::defaultConfig());
    void shutdown();
    
    // Loop principal
    bool runFrame();
    void runFrame();
    void runFrameWithMetrics();
    
    // Controle de qualidade
    void setQualityPreset(QualityPreset preset);
    QualityPreset getQualityPreset() const { return current_preset_; }
    void setGraphicsConfig(const GraphicsQualityConfig& config);
    const GraphicsQualityConfig& getGraphicsConfig() const { return graphics_config_; }
    
    // Controle do jogo
    bool loadGame(const std::string& nsp_path);
    bool loadSave(const std::string& save_path);
    bool saveGame(const std::string& save_path);
    void pause();
    void resume();
    void stop();
    
    // Input
    void onTouch(float x, float y, bool pressed);
    void onKey(int key_code, bool pressed);
    void onGamepad(int controller_id, uint32_t buttons_pressed, uint32_t buttons_released, float lx, float ly, float rx, float ry);
    
    // Qualidade dinâmica
    void setQualityPreset(QualityPreset preset);
    void setResolutionScale(float scale);
    void setSharpness(float sharpness);
    void setFSREnabled(bool enabled);
    void setTAAEnabled(bool enabled);
    void setRCASEnabled(bool enabled);
    
    // Configurações avançadas
    void setLODBias(float bias);
    void setMaxPolygonsPerFrame(uint32_t max);
    void setTargetFPS(int fps);
    void setVSync(bool enabled);
    
    // Métricas e debug
    const PerformanceStats& getPerformanceStats() const { return stats_; }
    void setDebugOverlayCallback(std::function<void(const PerformanceStats&)> callback);
    void toggleDebugOverlay(bool enabled);
    
    // Save states
    bool saveState(int slot);
    bool loadState(int slot);
    bool hasSaveState(int slot) const;
    
    // Screenshot
    bool captureScreenshot(const std::string& path);
    
    // Acesso aos subsistemas
    Emulator& getEmulator() { return *emulator_; }
    gpu::VulkanGpuExecutor& getGPU() { return *gpu_; }
    odyssey::OdysseyWorld& getWorld() { return *world_; }
    odyssey::OdysseyHandoffSource& getHandoff() { return *handoff_; }
    
    // Acesso ao Mental Map para debug
    const core::CameraMentalMapQuery& getCameraQuery() const { return camera_query_; }
    const core::MentalMapRuntime& getMentalMapRuntime() const { return world_.runtime(); }

private:
    std::unique_ptr<Emulator> emulator_;
    std::unique_ptr<gpu::VulkanGpuExecutor> gpu_;
    std::unique_ptr<FramebufferOptimizer> mfo_manager_;
    
    MarioOdysseyConfig config_;
    GraphicsQualityConfig graphics_config_;
    QualityPreset current_preset_ = QualityPreset::Balanced;
    
    PerformanceStats stats_;
    std::chrono::steady_clock::time_point frame_start_;
    uint64_t frame_count_ = 0;
    bool running_ = false;
    bool initialized_ = false;
    bool paused_ = false;
    
    std::function<void(const PerformanceStats&)> debug_callback_;
    bool debug_overlay_enabled_ = false;
    
    odyssey::OdysseyWorld world_;
    odyssey::OdysseyHandoffSource handoff_;
    core::CameraMentalMapQuery camera_query_;
    
    std::chrono::steady_clock::time_point last_fps_update_;
    int fps_update_interval_ = 60;
    int frames_since_fps_update_ = 0;
    
    void updatePerformanceMetrics();
    void applyQualitySettings();
    void updateThermalState();
    
    bool loadGameInternal(const std::string& nsp_path);
    bool loadKeys(const std::string& keys_dir);
    void setupQualityPreset();
};

} // namespace emu
} // namespace mgd