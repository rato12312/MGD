#include "MarioOdysseyRunner.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

#include "core/gpu/SeedPredictor.h"

namespace mgd {
namespace emu {

MarioOdysseyRunner::MarioOdysseyRunner()
    : current_preset_(QualityPreset::Balanced)
    , running_(false)
    , initialized_(false)
    , paused_(false)
    , frame_count_(0)
    , frames_since_fps_update_(0)
    , fps_update_interval_(60) {
}

MarioOdysseyRunner::~MarioOdysseyRunner() {
    shutdown();
}

bool MarioOdysseyRunner::initialize(const MarioOdysseyConfig& config) {
    config_ = config;
    graphics_config_ = config.graphics;
    current_preset_ = config.graphics.preset;

    // Load encryption keys
    if (!config.keys_dir.empty()) {
        if (!emulator_->loadKeys(config.keys_dir)) {
            std::cerr << "[WARN] Failed to load keys from " << config.keys_dir << std::endl;
        }
    }

    // Load Odyssey offsets if provided
    if (!config.keys_dir.empty()) {
        std::string offsets_file = config.keys_dir + "/odyssey_offsets.json";
        emulator_->loadOdysseyOffsets(offsets_file);
    }

    // Inicializa emulador
    emulator_ = std::make_unique<Emulator>();
    emulator_->applySwitches();

    // Inicializa GPU Vulkan
    gpu_ = std::make_unique<gpu::VulkanGpuExecutor>();
    uint32_t rw, rh, fw, fh;
    getGpuResolution(rw, rh, fw, fh);
    if (!gpu_->init(nullptr)) {
        return false;
    }
    gpu_->setResolution(rw, rh, fw, fh);

    // Inicializa Framebuffer Optimizer
    mfo_manager_ = std::make_unique<FramebufferOptimizer>();
    if (!mfo_manager_->init(gpu_->getVulkanContext(), fw, fh)) {
        return false;
    }

    // Inicializa Seed Predictor
    gpu_->initSeedPredictor(&emulator_->getCameraQuery(), &emulator_->getWorld().runtime());

    // Aplica configurações de qualidade
    applyQualitySettings();

    initialized_ = true;
    running_ = true;
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

    if (mfo_manager_) {
        mfo_manager_.reset();
    }

    initialized_ = false;
    running_ = false;
    paused_ = false;
}

bool MarioOdysseyRunner::runFrame() {
    if (!initialized_ || !running_ || paused_) return false;

    frame_start_ = std::chrono::steady_clock::now();

    char frame_path[64];
    snprintf(frame_path, sizeof(frame_path), "/sdcard/mgd_frame_%llu.ppm", frame_count_);

    // Atualiza Seed Predictor com estado atual do jogo
    if (gpu_ && gpu_->getSeedPredictor()) {
        SeedPredictor::GameStateSnapshot snapshot;
        // Mario state
        auto& world = emulator_->getWorld();
        auto& runtime = world.runtime();
        auto& camera = runtime.pipeline().camera();
        snapshot.mario_x = camera.position.x;
        snapshot.mario_y = camera.position.y;
        snapshot.mario_z = camera.position.z;
        // Camera state
        snapshot.cam_x = camera.position.x;
        snapshot.cam_y = camera.position.y;
        snapshot.cam_z = camera.position.z;
        snapshot.cam_pitch = camera.pitch;
        snapshot.cam_yaw = camera.yaw;
        snapshot.cam_roll = camera.roll;
        snapshot.cam_fov = camera.fov_degrees;
        snapshot.frame_index = frame_count_;
        
        gpu_->updateSeedPredictor(snapshot);
        gpu_->processSeedPredictions(frame_count_);
    }

    bool ok = emulator_->frame(frame_path, 64);

    if (ok) {
        frame_count_++;
    }

    updatePerformanceMetrics();
    return ok;
}

void MarioOdysseyRunner::runFrameWithMetrics() {
    auto frame_start = std::chrono::high_resolution_clock::now();

    runFrame();

    auto frame_end = std::chrono::high_resolution_clock::now();
    double frame_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();

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

    gpu_->setQualityPreset(current_preset_);

    if (auto* painter = gpu_->getPainter()) {
        painter->setFSREnabled(graphics_config_.enable_fsr);
        painter->setSharpness(graphics_config_.sharpness);

        if (graphics_config_.enable_taa) {
            // TAA handled in painter
        }
        if (graphics_config_.enable_rcas) {
            // RCAS handled in painter
        }
    }

    camera_query_.setViewDistance(graphics_config_.enable_lod ? 100.0f : 1000.0f);
    camera_query_.enableLOD(graphics_config_.enable_lod);
    camera_query_.setLODDistances(20.0f, 60.0f);
}

void MarioOdysseyRunner::setResolutionScale(float scale) {
    graphics_config_.resolution_scale = std::clamp(scale, 0.25f, 1.0f);
    applyQualitySettings();
}

void MarioOdysseyRunner::setSharpness(float sharpness) {
    graphics_config_.sharpness = std::clamp(sharpness, 0.0f, 1.0f);
    if (gpu_ && gpu_->getPainter()) gpu_->getPainter()->setSharpness(graphics_config_.sharpness);
}

void MarioOdysseyRunner::setFSREnabled(bool enabled) {
    graphics_config_.enable_fsr = enabled;
    if (gpu_ && gpu_->getPainter()) gpu_->getPainter()->setFSREnabled(enabled);
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

    // Use proper NSP boot with keys
    int header_key_slot = 0; // slot 0 for header key
    int section_key_slots[4] = {1, 2, 3, 4}; // slots 1-4 for sections
    
    return emulator_->bootNspWithKeys(buffer.data(), buffer.size(), 0, section_key_slots);
}

bool MarioOdysseyRunner::loadSave(const std::string& save_path) {
    return false;
}

bool MarioOdysseyRunner::saveGame(const std::string& save_path) {
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
    if (pressed) {
        emulator_->kernel().hid().press(0, 1 << 0);
    } else {
        emulator_->kernel().hid().release(0, 1 << 0);
    }
}

void MarioOdysseyRunner::onKey(int key_code, bool pressed) {
    if (!emulator_) return;

    uint32_t btn = 0;
    switch (key_code) {
        case 96: btn = 1 << 0; break;
        case 97: btn = 1 << 1; break;
        case 19: btn = 1 << 4; break;
        case 20: btn = 1 << 5; break;
        case 21: btn = 1 << 6; break;
        case 22: btn = 1 << 7; break;
        default: return;
    }

    if (pressed) {
        emulator_->kernel().hid().press(0, btn);
    } else {
        emulator_->kernel().hid().release(0, btn);
    }
}

void MarioOdysseyRunner::onGamepad(int controller_id, uint32_t buttons_pressed, uint32_t buttons_released, float lx, float ly, float rx, float ry) {
    (void)controller_id;
    (void)buttons_pressed;
    (void)buttons_released;
    (void)lx;
    (void)ly;
    (void)rx;
    (void)ry;
}

void MarioOdysseyRunner::setDebugOverlayCallback(std::function<void(const PerformanceStats&)> callback) {
    debug_callback_ = callback;
    debug_overlay_enabled_ = true;
}

void MarioOdysseyRunner::toggleDebugOverlay(bool enabled) {
    debug_overlay_enabled_ = enabled;
}

bool MarioOdysseyRunner::saveState(int slot) {
    return false;
}

bool MarioOdysseyRunner::loadState(int slot) {
    return false;
}

bool MarioOdysseyRunner::hasSaveState(int slot) const {
    return false;
}

bool MarioOdysseyRunner::captureScreenshot(const std::string& path) {
    return false;
}

void MarioOdysseyRunner::updatePerformanceMetrics() {
    if (!gpu_) return;

    stats_.draw_calls = gpu_->totalDrawCalls();
    stats_.vram_used_mb = gpu_->totalBytesExecuted() / (1024 * 1024);

    updateThermalState();

    frames_since_fps_update_++;
    if (frames_since_fps_update_ >= fps_update_interval_) {
        frames_since_fps_update_ = 0;
        if (debug_overlay_enabled_ && debug_callback_) {
            debug_callback_(stats_);
        }
    }
}

void MarioOdysseyRunner::updateThermalState() {
    stats_.temperature_c = 35.0f;
    stats_.thermal_throttling = stats_.temperature_c > 45.0f;
}

void MarioOdysseyRunner::getGpuResolution(uint32_t& roughW, uint32_t& roughH, uint32_t& finalW, uint32_t& finalH) const {
    auto cm = graphics_config_;
    if (cm.resolution_scale >= 1.0f) {
        roughW = 1280; roughH = 720; finalW = 1280; finalH = 720;
    } else {
        roughW = static_cast<uint32_t>(1280 * cm.resolution_scale);
        roughH = static_cast<uint32_t>(720 * cm.resolution_scale);
        finalW = 1280; finalH = 720;
    }
}

} // namespace emu
} // namespace mgd