#pragma once

// nvdrv:a (GPU) — backend real Vulkan + fallback stub para CI sem Vulkan.

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>

#include "Session.h"
#include "../gpu/Gpu.h"
#include "../gpu/VulkanBackend.h"

namespace mgd {
namespace hos {

class NvService {
public:
    NvService() = default;

    // Inicializa backend Vulkan (opcional: window_handle para swapchain)
    bool initVulkan(void* window_handle = nullptr) {
        vk_executor_ = std::make_unique<gpu::VulkanGpuExecutor>();
        return vk_executor_->init(window_handle);
    }

    // Inicializa com device/swapchain existentes (Android)
    bool initVulkanFromExisting(VkDevice device, VkPhysicalDevice physical_device,
                                 VkQueue graphics_queue, VkQueue present_queue,
                                 VkSurfaceKHR surface, VkSwapchainKHR swapchain) {
        vk_executor_ = std::make_unique<gpu::VulkanGpuExecutor>();
        return vk_executor_->initFromExisting(device, physical_device, graphics_queue, present_queue, surface, swapchain);
    }

    void shutdownVulkan() {
        if (vk_executor_) {
            vk_executor_->shutdown();
            vk_executor_.reset();
        }
    }

    // cmd 1 = Open: payload[0] = device tag; responde id do canal ou 0.
    // cmd 2 = Close: payload = id; responde 1 ok / 0 inválido.
    // cmd 3 = Submit: payload = id(4) + bytes do command buffer;
    //         enfileira e responde fence id. Execução via VulkanGpuExecutor.
    // cmd 4 = QueryFence: payload = fence; responde 1 pronto / 0 fila.
    // cmd 5 = GetFence: payload = fence; responde 1 pronto / 0 fila (alias).
    // cmd 6 = WaitFence: payload = fence; bloqueia até pronto (simulado).
    // cmd 7 = GetInfo: payload = 0; responde info do driver.
    // cmd 8 = CreateTexture: payload = w(4) h(4) fmt(4) data...
    // cmd 9 = CreateSampler: payload = min(4) mag(4) wrapS(4) wrapT(4)
    // cmd 10 = CreateRenderTarget: payload = w(4) h(4)
    // cmd 11 = CreateShader: payload = stage(1) bytecode...
    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            uint32_t tag = req.payload.empty() ? 0 : req.payload[0];
            uint32_t id = next_channel_++;
            channels_[id] = tag;
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(id & 0xFF),
                           static_cast<uint8_t>((id >> 8) & 0xFF),
                           static_cast<uint8_t>((id >> 16) & 0xFF),
                           static_cast<uint8_t>((id >> 24) & 0xFF)};
            return true;
        }
        if (req.cmd == 2) {
            if (req.payload.size() < 4) {
                rep.cmd = 0;
                return true;
            }
            uint32_t id = rd32(req.payload, 0);
            rep.cmd = channels_.erase(id) > 0 ? 1 : 0;
            return true;
        }
        if (req.cmd == 3) {
            if (req.payload.size() < 4) {
                rep.cmd = 0;
                return true;
            }
            uint32_t id = rd32(req.payload, 0);
            if (channels_.find(id) == channels_.end()) {
                rep.cmd = 0;
                return true;
            }
            pending_.push_back(Pending{id, next_fence_++});
            if (req.payload.size() > 4) {
                size_t n = req.payload.size() - 4;
                if (n > 65536) n = 65536;
                last_submit_.assign(req.payload.begin() + 4, req.payload.begin() + 4 + n);
                submit_count_++;
                // Execute command buffer via Vulkan backend (or stub)
                if (vk_executor_) {
                    vk_executor_->execute(last_submit_.data(), last_submit_.size());
                } else {
                    executor_.execute(last_submit_.data(), last_submit_.size());
                }
            }
            rep.cmd = 1;
            uint32_t f = next_fence_ - 1;
            rep.payload = {static_cast<uint8_t>(f & 0xFF),
                           static_cast<uint8_t>((f >> 8) & 0xFF),
                           static_cast<uint8_t>((f >> 16) & 0xFF),
                           static_cast<uint8_t>((f >> 24) & 0xFF)};
            return true;
        }
        if (req.cmd == 4 || req.cmd == 5) {
            if (req.payload.size() < 4) {
                rep.cmd = 0;
                return true;
            }
            uint32_t f = rd32(req.payload, 0);
            rep.cmd = (f < completed_fence_) ? 1 : 0;
            return true;
        }
        if (req.cmd == 6) {
            if (req.payload.size() < 4) {
                rep.cmd = 0;
                return true;
            }
            for (auto& p : pending_) {
                if (p.fence <= rd32(req.payload, 0)) {
                    completeUpTo(p.fence);
                }
            }
            rep.cmd = 1;
            return true;
        }
        if (req.cmd == 7) {
            rep.cmd = 1;
            rep.payload = {0x01, 0x00, 0x00, 0x00};
            return true;
        }
        // Extended GPU resource creation (game can call these via ioctl)
        auto& exec = vk_executor_ ? *vk_executor_ : executor_;
        if (req.cmd == 8) { // CreateTexture
            if (req.payload.size() < 12) { rep.cmd = 0; return true; }
            uint32_t w = rd32(req.payload, 0);
            uint32_t h = rd32(req.payload, 4);
            uint32_t fmt = rd32(req.payload, 8);
            uint32_t tex_id = exec.createTexture(w, h, static_cast<gpu::TexFormat>(fmt),
                                                 req.payload.size() > 12 ? &req.payload[12] : nullptr,
                                                 req.payload.size() > 12 ? req.payload.size() - 12 : 0);
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(tex_id & 0xFF),
                           static_cast<uint8_t>((tex_id >> 8) & 0xFF),
                           static_cast<uint8_t>((tex_id >> 16) & 0xFF),
                           static_cast<uint8_t>((tex_id >> 24) & 0xFF)};
            return true;
        }
        if (req.cmd == 9) { // CreateSampler
            if (req.payload.size() < 16) { rep.cmd = 0; return true; }
            uint32_t min_f = rd32(req.payload, 0);
            uint32_t mag_f = rd32(req.payload, 4);
            uint32_t wrap_s = rd32(req.payload, 8);
            uint32_t wrap_t = rd32(req.payload, 12);
            uint32_t samp_id = exec.createSampler(min_f, mag_f, wrap_s, wrap_t);
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(samp_id & 0xFF),
                           static_cast<uint8_t>((samp_id >> 8) & 0xFF),
                           static_cast<uint8_t>((samp_id >> 16) & 0xFF),
                           static_cast<uint8_t>((samp_id >> 24) & 0xFF)};
            return true;
        }
        if (req.cmd == 10) { // CreateRenderTarget
            if (req.payload.size() < 8) { rep.cmd = 0; return true; }
            uint32_t w = rd32(req.payload, 0);
            uint32_t h = rd32(req.payload, 4);
            uint32_t rt_id = exec.createRenderTarget(w, h);
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(rt_id & 0xFF),
                           static_cast<uint8_t>((rt_id >> 8) & 0xFF),
                           static_cast<uint8_t>((rt_id >> 16) & 0xFF),
                           static_cast<uint8_t>((rt_id >> 24) & 0xFF)};
            return true;
        }
        if (req.cmd == 11) { // CreateShader
            if (req.payload.size() < 5) { rep.cmd = 0; return true; }
            uint8_t stage = req.payload[0];
            std::vector<uint32_t> code((req.payload.size() - 1) / 4);
            for (size_t i = 0; i < code.size(); i++) code[i] = rd32(req.payload, 1 + i * 4);
            uint32_t shader_id = exec.createShader(static_cast<gpu::ShaderStage>(stage), code);
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(shader_id & 0xFF),
                           static_cast<uint8_t>((shader_id >> 8) & 0xFF),
                           static_cast<uint8_t>((shader_id >> 16) & 0xFF),
                           static_cast<uint8_t>((shader_id >> 24) & 0xFF)};
            return true;
        }
        return false;
    }

    // ===== Quality Presets =====
    enum class QualityPreset {
        Ultra,      // 720p nativo, sem upscale
        Quality,    // 960x540 -> FSR 2.x Quality -> 720p
        Balanced,   // 854x480 -> FSR 2.x Balanced
        Performance // 512x288 -> FSR 2.x Performance (atual)
    };

    struct QualityConfig {
        uint32_t rough_w = 512;
        uint32_t rough_h = 288;
        uint32_t final_w = 1280;
        uint32_t final_h = 720;
        bool use_fsr = false;
        float sharpness = 0.5f;
    };

    // Renderer nativo na resolução configurada
    void renderFrameNative() {
        // Drena todos os command buffers pendentes executando-os nativamente
        while (!pending_.empty()) {
            completeUpTo(pending_.front().fence);
        }
    }

    // Presets de qualidade
    void setQualityPreset(QualityPreset preset) {
        switch (preset) {
            case QualityPreset::Ultra:
                rough_w_ = 1280; rough_h_ = 720;
                final_w_ = 1280; final_h_ = 720;
                use_fsr_ = false; sharpness_ = 0.0f;
                break;
            case QualityPreset::Quality:
                rough_w_ = 960; rough_h_ = 540;
                final_w_ = 1280; final_h_ = 720;
                use_fsr_ = true; sharpness_ = 0.3f;
                break;
            case QualityPreset::Balanced:
                rough_w_ = 854; rough_h_ = 480;
                final_w_ = 1280; final_h_ = 720;
                use_fsr_ = true; sharpness_ = 0.5f;
                break;
            case QualityPreset::Performance:
                rough_w_ = 512; rough_h_ = 288;
                final_w_ = 1280; final_h_ = 720;
                use_fsr_ = true; sharpness_ = 0.7f;
                break;
        }
        if (vk_executor_) {
            vk_executor_->setResolution(rough_w_, rough_h_, final_w_, final_h_);
        }
    }

    uint32_t roughWidth() const { return rough_w_; }
    uint32_t roughHeight() const { return rough_h_; }
    uint32_t finalWidth() const { return final_w_; }
    uint32_t finalHeight() const { return final_h_; }
    bool useFSR() const { return use_fsr_; }
    float sharpness() const { return sharpness_; }

    // Stats from executor
    uint64_t drawCalls() const {
        return vk_executor_ ? vk_executor_->totalDrawCalls() : executor_.totalDrawCalls();
    }
    uint64_t computeDispatches() const {
        return vk_executor_ ? vk_executor_->totalComputeDispatches() : executor_.totalComputeDispatches();
    }
    uint64_t bytesExecuted() const {
        return vk_executor_ ? vk_executor_->totalBytesExecuted() : executor_.totalBytesExecuted();
    }

    size_t channelCount() const { return channels_.size(); }
    size_t pendingCount() const { return pending_.size(); }
    uint64_t submitCount() const { return submit_count_; }
    const std::vector<uint8_t>& lastSubmit() const { return last_submit_; }

private:
    static uint32_t rd32(const std::vector<uint8_t>& v, size_t o) {
        uint32_t r = 0;
        for (int i = 0; i < 4; i++) r |= static_cast<uint32_t>(v[o + i]) << (8 * i);
        return r;
    }

    struct Pending {
        uint32_t channel = 0;
        uint32_t fence = 0;
    };
    std::unordered_map<uint32_t, uint32_t> channels_;
    std::vector<Pending> pending_;
    std::vector<uint8_t> last_submit_;
    uint64_t submit_count_ = 0;
    uint32_t next_channel_ = 1;
    uint32_t next_fence_ = 1;
    uint32_t completed_fence_ = 0;
    gpu::GpuExecutor executor_; // Fallback stub
    std::unique_ptr<gpu::VulkanGpuExecutor> vk_executor_;

    uint32_t rough_w_ = 512, rough_h_ = 288;
    uint32_t final_w_ = 1280, final_h_ = 720;
    bool use_fsr_ = true;
    float sharpness_ = 0.5f;

    void completeUpTo(uint32_t f) {
        if (f > completed_fence_) completed_fence_ = f;
        while (!pending_.empty() && pending_.front().fence <= completed_fence_)
            pending_.erase(pending_.begin());
    }
};

} // namespace hos
} // namespace mgd
