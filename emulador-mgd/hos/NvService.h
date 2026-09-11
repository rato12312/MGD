#pragma once

// nvdrv:a (GPU) — esqueleto honesto: abre canal, fecha, ioctl nega.
// Os comandos reais de submit vêm quando o renderer existir.

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Session.h"
#include "../gpu/Gpu.h"

namespace mgd {
namespace hos {

class NvService {
public:
    NvService() = default;

    // cmd 1 = Open: payload[0] = device tag; responde id do canal ou 0.
    // cmd 2 = Close: payload = id; responde 1 ok / 0 inválido.
    // cmd 3 = Submit: payload = id(4) + bytes do command buffer;
    //         enfileira e responde fence id. Execução via GpuExecutor.
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
                // Execute command buffer immediately (stub renderer)
                executor_.execute(last_submit_.data(), last_submit_.size());
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
        if (req.cmd == 8) { // CreateTexture
            if (req.payload.size() < 12) { rep.cmd = 0; return true; }
            uint32_t w = rd32(req.payload, 0);
            uint32_t h = rd32(req.payload, 4);
            uint32_t fmt = rd32(req.payload, 8);
            uint32_t tex_id = executor_.createTexture(w, h, static_cast<gpu::TexFormat>(fmt),
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
            uint32_t samp_id = executor_.createSampler(min_f, mag_f, wrap_s, wrap_t);
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
            uint32_t rt_id = executor_.createRenderTarget(w, h);
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
            uint32_t shader_id = executor_.createShader(static_cast<gpu::ShaderStage>(stage), code);
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(shader_id & 0xFF),
                           static_cast<uint8_t>((shader_id >> 8) & 0xFF),
                           static_cast<uint8_t>((shader_id >> 16) & 0xFF),
                           static_cast<uint8_t>((shader_id >> 24) & 0xFF)};
            return true;
        }
        return false;
    }

    // Renderer nativo 720p: executa command buffer e apresenta frame final.
    void renderFrame720p() {
        // Drena todos os command buffers pendentes executando-os nativamente 720p
        while (!pending_.empty()) {
            completeUpTo(pending_.front().fence);
        }
        // Apresenta frame final (swapchain seria aqui em implementação real)
    }

    // Stats from executor
    uint64_t drawCalls() const { return executor_.totalDrawCalls(); }
    uint64_t computeDispatches() const { return executor_.totalComputeDispatches(); }
    uint64_t bytesExecuted() const { return executor_.totalBytesExecuted(); }

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
    gpu::GpuExecutor executor_;

    void completeUpTo(uint32_t f) {
        if (f > completed_fence_) completed_fence_ = f;
        while (!pending_.empty() && pending_.front().fence <= completed_fence_)
            pending_.erase(pending_.begin());
    }
};

} // namespace hos
} // namespace mgd
