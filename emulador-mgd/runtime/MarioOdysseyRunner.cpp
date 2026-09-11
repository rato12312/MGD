#include "MarioOdysseyRunner.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace mgd {
namespace emu {

MarioOdysseyRunner::MarioOdysseyRunner() 
    : current_preset_(QualityPreset::Balanced)
    , running_(false)
    , initialized_(false)
    , paused_(false)
    , frame_count_(0) {
}

MarioOdysseyRunner::~MarioOdysseyRunner() {
    shutdown();
}

bool MarioOdysseyRunner::initialize(const MarioOdysseyConfig& config) {
    config_ = config;
    graphics_config_ = config.graphics;
    current_preset_ = config.graphics.preset;
    
    // Inicializa emulador
    emulator_ = std::make_unique<Emulator>();
    emulator_->applySwitches();
    
    // Configura qualidade gráfica
    graphics_config_ = config.graphics;
    
    // Inicializa GPU
    gpu_ = std::make_unique<gpu::VulkanGpuExecutor>();
    uint32_t rw, rh, fw, fh;
    getGpuResolution(rw, rh, fw, fh);
    if (!gpu_->init(nullptr)) {
        return false;
    }
    gpu_->setResolution(rw, rh, fw, fh);
    
    // Inicializa Framebuffer Optimizer
    mfo_manager_ = std::make_unique<FramebufferOptimizer>();
    if (!mfo_manager_->init(nullptr, 1280, 720)) {
        return false;
    }
    
    // Aplica configurações de qualidade
    applyQualitySettings();
    
    // Inicializa emulador
    emulator_ = std::make_unique<Emulator>();
    emulator_->applySwitches();
    emulator_->setSvcHost(&kernel_);
    
    initialized_ = true;
    return true;
}

bool MarioOdysseyRunner::initialize(const MarioOdysseyConfig& config) {
    config_ = config;
    graphics_config_ = config.graphics;
    current_preset_ = config.graphics.preset;
    
    if (!initialize(config)) {
        return false;
    }
    
    // Carrega jogo se especificado
    if (config.auto_load && !config.nsp_path.empty()) {
        if (!loadGame(config.nsp_path)) {
            return false;
        }
    }
    
    if (config.auto_load) {
        loadGame(config.nsp_path);
    }
    
    return true;
}

void MarioOdysseyRunner::shutdown() {
    running_ = false;
    
    if (gpu_) {
        gpu_->shutdown();
        gpu_.reset();
    }
    
    if (emulator_) {
        emulator_.reset();
    }
    
    initialized_ = false;
    running_ = false;
}

bool MarioOdysseyRunner::runFrame() {
    if (!initialized_ || !running_ || paused_) return false;
    
    frame_start_ = std::chrono::steady_clock::now();
    
    // Frame do emulador com qualidade configurada
    char frame_path[64];
    snprintf(frame_path, sizeof(frame_path), "/sdcard/mgd_frame_%llu.ppm", frame_count_);
    
    bool ok = frame("/sdcard/mgd_frame.ppm", 64); // Usa qualidade configurada
    
    if (ok) {
        frame_count_++;
    }
    
    updatePerformanceMetrics();
    return true;
}

void MarioOdysseyRunner::runFrame() {
    if (!running_ || paused_) return;
    
    frame_start_ = std::chrono::steady_clock::now();
    
    // Frame do emulador com qualidade configurada
    char frame_path[64];
    snprintf(frame_path, sizeof(frame_path), "/sdcard/mgd_frame_%llu.ppm", frame_count_);
    
    bool ok = frame("/sdcard/mgd_frame.ppm", 64); // Qualidade baseada no preset
    
    if (ok) {
        frame_count_++;
    }
    
    updatePerformanceMetrics();
}

void MarioOdysseyRunner::runFrameWithMetrics() {
    auto frame_start = std::chrono::high_resolution_clock::now();
    
    runFrame();
    
    auto frame_end = std::chrono::high_resolution_clock::now();
    double frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start_).count();
    
    stats_.frame_time_ms = frame_ms;
    stats_.current_fps = frame_ms > 0 ? 1000.0 / frame_ms : 0.0;
    stats_.frame_count = frame_count_;
    
    // Atualiza FPS médio (média móvel)
    if (frame_count_ == 1) {
        stats_.avg_fps = stats_.current_fps;
    } else {
        stats_.avg_fps = stats_.avg_fps * 0.9 + stats_.current_fps * 0.1;
    }
    
    if (stats_.current_fps < stats_.min_fps) stats_.min_fps = stats_.current_fps;
    if (stats_.current_fps > stats_.max_fps) stats_.max_fps = stats_.current_fps;
    
    // Atualiza overlay de debug se habilitado
    if (debug_overlay_enabled_ && debug_callback_) {
        debug_callback_(stats_);
    }
}

void MarioOdysseyRunner::runFrame() {
    runFrameWithMetrics();
}

void MarioOdysseyRunner::setQualityPreset(QualityPreset preset) {
    current_preset_ = preset;
    graphics_config_ = GraphicsQualityConfig::fromPreset(preset);
    applyQualitySettings();
}

void MarioOdysseyRunner::setGraphicsConfig(const GraphicsQualityConfig& config) {
    graphics_config_ = config;
    applyQualitySettings();
}

void MarioOdysseyRunner::applyQualitySettings() {
    if (!gpu_) return;
    
    // Configura preset no GPU executor
    gpu_->setQualityPreset(current_preset_);
    
    // Configura Painter
    if (gpu_ && gpu_->vk_ctx_) {
        auto& painter = gpu_->painter_;
        painter.setFSREnabled(graphics_config_.enable_fsr);
        painter->setSharpness(graphics_config_.sharpness);
        
        if (graphics_config_.enable_taa) {
            // Habilita TAA
        }
        if (graphics_config_.enable_rcas) {
            // Habilita RCAS
        }
    }
    
    // Configura Mental Map query
    camera_query_.setViewDistance(graphics_config_.enable_lod ? 100.0f : 1000.0f);
    camera_query_.enableLOD(graphics_config_.enable_lod);
    camera_query_.setLODDistances(20.0f, 60.0f);
}

void MarioOdysseyRunner::setQualityPreset(QualityPreset preset) {
    current_preset_ = preset;
    graphics_config_ = GraphicsQualityConfig::fromPreset(preset);
    applyQualitySettings();
}

void MarioOdysseyRunner::setGraphicsConfig(const GraphicsQualityConfig& config) {
    graphics_config_ = config;
    applyQualitySettings();
}

void MarioOdysseyRunner::setResolutionScale(float scale) {
    graphics_config_.resolution_scale = std::clamp(scale, 0.25f, 1.0f);
    applyQualitySettings();
}

void MarioOdysseyRunner::setSharpness(float sharpness) {
    graphics_config_.sharpness = std::clamp(sharpness, 0.0f, 1.0f);
    if (gpu_ && gpu_->vk_ctx_) {
        gpu_->painter_.setSharpness(graphics_config_.sharpness);
    }
}

void MarioOdysseyRunner::setFSREnabled(bool enabled) {
    graphics_config_.enable_fsr = enabled;
    if (gpu_) gpu->painter_.setFSREnabled(enabled);
}

void MarioOdysseyRunner::setTAAEnabled(bool enabled) {
    graphics_config_.enable_taa = enabled;
}

void MarioOdysseyRunner::setRCASEnabled(bool enabled) {
    graphics_config_.enable_rcas = enabled;
}

void MarioOdysseyRunner::setLODBias(float bias) {
    graphics_config_.lod_bias = std::clamp(bias, -1.0f, 1.0f);
}

void MarioOdysseyRunner::setMaxPolygonsPerFrame(uint32_t max) {
    graphics_config_.max_polygons_per_frame = max;
}

void MarioOdysseyRunner::setTargetFPS(int fps) {
    config_.target_fps = fps;
}

void MarioOdysseyRunner::setVSync(bool enabled) {
    graphics_config_.vsync = enabled;
}

bool MarioOdysseyRunner::loadGame(const std::string& nsp_path) {
    if (!emulator_) return false;
    
    std::ifstream file(nsp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return emulator->bootNsp(buffer.data(), buffer.size());
}

bool MarioOdysseyRunner::loadGame(const std::string& nsp_path) {
    std::ifstream file(nsp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return emulator->bootNsp(buffer.data(), buffer.size());
}

bool MarioOdysseyRunner::loadSave(const std::string& save_path) {
    // Implementar carregamento de save state
    return false;
}

bool MarioOdysseyRunner::saveGame(const std::string& save_path) {
    // Implementar salvamento de save state
    return false;
}

void MarioOdysseyRunner::pause() {
    paused_ = true;
}

void MarioOdysseyRunner::resume() {
    paused_ = false;
}

void MarioOdysseyRunner::stop() {
    running_ = false;
    paused_ = false;
}

void MarioOdysseyRunner::onTouch(float x, float y, bool pressed) {
    if (!emulator_) return;
    // Mapear toque para HID
    if (pressed) {
        emulator->kernel().hid().press(0, 1 << 0); // A button
    } else {
        emulator->kernel().hid().release(0, 1 << 0);
    }
}

void MarioOdysseyRunner::onKey(int key_code, bool pressed) {
    if (!emulator_) return;
    
    uint32_t btn = 0;
    switch (key_code) {
        case 96: // AKEYCODE_BUTTON_A
            btn = 1 << 0; break;
        case 97: // AKEYCODE_BUTTON_B
            btn = 1 << 1; break;
        case 19: // AKEYCODE_DPAD_UP
            btn = 1 << 4; break;
        case 20: // AKEYCODE_DPAD_DOWN
            btn = 1 << 5; break;
        case 21: // AKEYCODE_DPAD_LEFT
            btn = 1 << 6; break;
        case 22: // AKEYCODE_DPAD_RIGHT
            btn = 1 << 5; break;
        default:
            return;
    }
    
    if (pressed) {
        emulator->kernel().hid().press(0, btn);
    } else {
        emulator->kernel().hid().release(0, btn);
    }
}

void MarioOdysseyRunner::onGamepad(int controller_id, uint32_t buttons_pressed, uint32_t buttons_released, float lx, float ly, float rx, float ry) {
    if (!emulator_) return;
    // Implementar mapeamento de gamepad para HID
}

void MarioOdysseyRunner::setResolutionScale(float scale) {
    graphics_config_.resolution_scale = std::clamp(scale, 0.25f, 1.0f);
    applyQualitySettings();
}

void MarioOdysseyRunner::setSharpness(float sharpness) {
    graphics_config_.sharpness = std::clamp(sharpness, 0.0f, 1.0f);
    if (gpu_) gpu->painter_.setSharpness(graphics_config_.sharpness);
}

void MarioOdysseyRunner::setFSREnabled(bool enabled) {
    graphics_config_.enable_fsr = enabled;
    if (gpu_) gpu->painter_.setFSREnabled(enabled);
}

void MarioOdysseyRunner::setTAAEnabled(bool enabled) {
    graphics_config_.enable_taa = enabled;
}

void MarioOdysseyRunner::setRCASEnabled(bool enabled) {
    graphics_config_.enable_rcas = enabled;
}

void MarioOdysseyRunner::setLODBias(float bias) {
    graphics_config_.lod_bias = std::clamp(bias, -1.0f, 1.0f);
}

void MarioOdysseyRunner::setMaxPolygonsPerFrame(uint32_t max) {
    graphics_config_.max_polygons_per_frame = max;
}

void MarioOdysseyRunner::setTargetFPS(int fps) {
    config_.target_fps = fps;
}

void MarioOdysseyRunner::setVSync(bool enabled) {
    graphics_config_.vsync = enabled;
}

void MarioOdysseyRunner::setDebugOverlayCallback(std::function<void(const PerformanceStats&)> callback) {
    debug_callback_ = callback;
    debug_overlay_enabled_ = true;
}

void MarioOdysseyRunner::toggleDebugOverlay(bool enabled) {
    debug_overlay_enabled_ = enabled;
}

bool MarioOdysseyRunner::saveState(int slot) {
    // Implementar save state
    return false;
}

bool MarioOdysseyRunner::loadState(int slot) {
    // Implementar load state
    return false;
}

bool MarioOdysseyRunner::hasSaveState(int slot) const {
    return false;
}

bool MarioOdysseyRunner::captureScreenshot(const std::string& path) {
    // Captura screenshot via framebuffer
    return false;
}

bool MarioOdysseyRunner::initializeSubsystems() {
    return true;
}

void MarioOdysseyRunner::updatePerformanceMetrics() {
    auto now = std::chrono::steady_clock::now();
    double frame_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - frame_start_).count();
    
    stats_.frame_time_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - frame_start_).count();
    stats_.current_fps = stats_.frame_time_ms > 0 ? 1000.0 / stats_.frame_time_ms : 0.0;
    stats_.frame_count = frame_count_;
    
    if (frame_count_ == 1) {
        stats_.avg_fps = stats_.current_fps;
    } else {
        stats_.avg_fps = stats_.avg_fps * 0.9 + stats_.current_fps * 0.1;
    }
    
    if (stats_.current_fps < stats_.min_fps) stats_.min_fps = stats_.current_fps;
    if (stats_.current_fps > stats_.max_fps) stats_.max_fps = stats_.current_fps;
    
    // GPU stats
    if (gpu_) {
        stats_.draw_calls = gpu_->totalDrawCalls();
        stats_.vram_used_mb = 0; // TODO: query real VRAM
    }
    
    // Update debug overlay
    if (debug_overlay_enabled_ && debug_callback_) {
        debug_callback_(stats_);
    }
    
    // Update FPS display periodically
    frames_since_fps_update_++;
    if (frames_since_fps_update_ >= fps_update_interval_) {
        frames_since_fps_update_ = 0;
    }
}

void MarioOdysseyRunner::applyQualitySettings() {
    if (!gpu_) return;
    
    gpu_->setQualityPreset(current_preset_);
    gpu->painter_.setFSREnabled(graphics_config_.enable_fsr);
    gpu->painter_.setSharpness(graphics_config_.sharpness);
}

void MarioOdysseyRunner::updateQualitySettings() {
    applyQualitySettings();
}

void MarioOdysseyRunner::updatePerformanceMetrics() {
    // Update thermal state
    updateThermalState();
    
    // Update GPU stats
    if (gpu_) {
        stats_.draw_calls = gpu_->totalDrawCalls();
        stats_.vram_used_mb = gpu_->totalBytesExecuted() / (1024 * 1024);
    }
}

void MarioOdysseyRunner::renderDebugOverlay() {
    if (debug_overlay_enabled_ && debug_callback_) {
        debug_callback_(stats_);
    }
}

void MarioOdysseyRunner::updateThermalState() {
    // TODO: Read thermal zone
    stats_.temperature_c = 35.0f; // placeholder
    stats_.thermal_throttling = stats_.temperature_c > 45.0f;
}

bool MarioOdysseyRunner::initializeSubsystems() {
    return true;
}

void MarioOdysseyRunner::updateQualitySettings() {
    applyQualitySettings();
}

void MarioOdysseyRunner::renderDebugOverlay() {
    if (debug_overlay_enabled_ && debug_callback_) {
        debug_callback_(stats_);
    }
}

void MarioOdysseyRunner::updateThermalState() {
    // Placeholder - ler thermal zone do Android
    stats_.temperature_c = 35.0f;
    stats_.thermal_throttling = stats_.temperature_c > 45.0f;
}

bool MarioOdysseyRunner::loadGameInternal(const std::string& nsp_path) {
    std::ifstream file(nsp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return emulator->bootNsp(buffer.data(), buffer.size());
}

bool MarioOdysseyRunner::loadGame(const std::string& nsp_path) {
    std::ifstream file(nsp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return emulator->bootNsp(buffer.data(), buffer.size());
}

bool MarioOdysseyRunner::loadSave(const std::string& save_path) {
    // TODO: Implement save state loading
    return false;
}

bool MarioOdysseyRunner::saveGame(const std::string& save_path) {
    // TODO: Implement save state
    return false;
}

bool MarioOdysseyRunner::captureScreenshot(const std::string& path) {
    // TODO: Implement screenshot capture
    return false;
}

void MarioOdysseyRunner::setQualityPreset(QualityPreset preset) {
    current_preset_ = preset;
    graphics_config_ = GraphicsQualityConfig::fromPreset(preset);
    applyQualitySettings();
}

void MarioOdysseyRunner::setGraphicsConfig(const GraphicsQualityConfig& config) {
    graphics_config_ = config;
    applyQualitySettings();
}

void MarioOdysseyRunner::setResolutionScale(float scale) {
    graphics_config_.resolution_scale = std::clamp(scale, 0.25f, 1.0f);
    applyQualitySettings();
}

void MarioOdysseyRunner::setSharpness(float sharpness) {
    graphics_config_.sharpness = std::clamp(sharpness, 0.0f, 1.0f);
    if (gpu_) gpu->painter_.setSharpness(graphics_config_.sharpness);
}

void MarioOdysseyRunner::setFSREnabled(bool enabled) {
    graphics_config_.enable_fsr = enabled;
    if (gpu_) gpu->painter_.setFSREnabled(enabled);
}

void MarioOdysseyRunner::setTAAEnabled(bool enabled) {
    graphics_config_.enable_taa = enabled;
}

void MarioOdysseyRunner::setRCASEnabled(bool enabled) {
    graphics_config_.enable_rcas = enabled;
}

void MarioOdysseyRunner::setLODBias(float bias) {
    graphics_config_.lod_bias = std::clamp(bias, -1.0f, 1.0f);
}

void MarioOdysseyRunner::setMaxPolygonsPerFrame(uint32_t max) {
    graphics_config_.max_polygons_per_frame = max;
}

void MarioOdysseyRunner::setTargetFPS(int fps) {
    config_.target_fps = fps;
}

void MarioOdysseyRunner::setVSync(bool enabled) {
    graphics_config_.vsync = enabled;
}

bool MarioOdysseyRunner::loadGame(const std::string& nsp_path) {
    return loadGameInternal(nsp_path);
}

bool MarioOdysseyRunner::loadGameInternal(const std::string& nsp_path) {
    std::ifstream file(nsp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return emulator->bootNsp(buffer.data(), buffer.size());
}

void MarioOdysseyRunner::updateQualitySettings() {
    applyQualitySettings();
}

void MarioOdysseyRunner::runFrame() {
    if (!running_ || paused_) return;
    
    frame_start_ = std::chrono::steady_clock::now();
    
    char frame_path[64];
    snprintf(frame_path, sizeof(frame_path), "/sdcard/mgd_frame_%llu.ppm", frame_count_);
    
    bool ok = frame(frame_path, 64);
    
    if (ok) {
        frame_count_++;
    }
    
    updatePerformanceMetrics();
}

void MarioOdysseyRunner::runFrameWithMetrics() {
    auto frame_start = std::chrono::high_resolution_clock::now();
    
    runFrame();
    
    auto frame_end = std::chrono::high_resolution_clock::now();
    double frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start_).count();
    
    stats_.frame_time_ms = frame_ms;
    stats_.current_fps = frame_ms > 0 ? 1000.0 / frame_ms : 0.0;
    stats_.frame_count = frame_count_;
    
    if (frame_count_ == 1) {
        stats_.avg_fps = stats_.current_fps;
    } else {
        stats_.avg_fps = stats_.avg_fps * 0.9 + stats_.current_fps * 0.1;
    }
    
    if (stats_.current_fps < stats_.min_fps) stats_.min_fps = stats_.current_fps;
    if (stats_.current_fps > stats_.max_fps) stats_.max_fps = stats_.current_fps;
    
    if (debug_overlay_enabled_ && debug_callback_) {
        debug_callback_(stats_);
    }
}

void MarioOdysseyRunner::updatePerformanceMetrics() {
    updateThermalState();
    
    if (gpu_) {
        stats_.draw_calls = gpu_->totalDrawCalls();
        stats_.vram_used_mb = gpu_->totalBytesExecuted() / (1024 * 1024);
    }
}

void MarioOdysseyRunner::renderDebugOverlay() {
    if (debug_overlay_enabled_ && debug_callback_) {
        debug_callback_(stats_);
    }
}

void MarioOdysseyRunner::updateThermalState() {
    // Placeholder - ler thermal zone do Android
    stats_.temperature_c = 35.0f; // placeholder
    stats_.thermal_throttling = stats_.temperature_c > 45.0f;
}

bool MarioOdysseyRunner::loadGameInternal(const std::string& nsp_path) {
    std::ifstream file(nsp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return emulator->bootNsp(buffer.data(), buffer.size());
}

void MarioOdysseyRunner::loadGame() {
    const char* nspPath = "/sdcard/MGD/game.nsp";
    loadGame("/sdcard/MGD/game.nsp");
}

bool MarioOdysseyRunner::initializeSubsystems() {
    return true;
}

void MarioOdysseyRunner::updateQualitySettings() {
    applyQualitySettings();
}

void MarioOdysseyRunner::renderDebugOverlay() {
    if (debug_overlay_enabled_ && debug_callback_) {
        debug_callback_(stats_);
    }
}

void MarioOdysseyRunner::updateThermalState() {
    stats_.temperature_c = 35.0f; // placeholder
    stats_.thermal_throttling = stats_.temperature_c > 45.0f;
}

bool MarioOdysseyRunner::loadGameInternal(const std::string& nsp_path) {
    std::ifstream file(nsp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    return emulator->bootNsp(buffer.data(), buffer.size());
}

void MarioOdysseyRunner::configureCameraQuery(float view_dist = 100.0f, bool lod = true) {
    camera_query_.setViewDistance(view_dist);
    camera_query_.enableLOD(lod);
}

} // namespace emu
} // namespace mgd