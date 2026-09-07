#pragma once

// nvdrv:a (GPU) — esqueleto honesto: abre canal, fecha, ioctl nega.
// Os comandos reais de submit vêm quando o renderer existir.

#include <cstdint>
#include <unordered_map>

#include "Session.h"

namespace mgd {
namespace hos {

class NvService {
public:
    NvService() = default;

    // cmd 1 = Open: payload[0] = device tag; responde id do canal ou 0.
    // cmd 2 = Close: payload = id; responde 1 ok / 0 inválido.
    // cmd 3 = Submit: payload = id(4) + bytes do command buffer;
    //         enfileira e responde fence id. Execução vem depois.
    // cmd 4 = QueryFence: payload = fence; responde 1 pronto / 0 fila.
    // cmd 5 = GetFence: payload = fence; responde 1 pronto / 0 fila (alias).
    // cmd 6 = WaitFence: payload = fence; bloqueia até pronto (simulado).
    // cmd 7 = GetInfo: payload = 0; responde info do driver.
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
            // Simula wait: avança fence até o pedido
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
            rep.payload = {0x01, 0x00, 0x00, 0x00}; // versão fake
            return true;
        }
        return false;
    }

    // Renderer nulo: drena N pendentes (execução = descarta, por enquanto).
    uint32_t drain(uint32_t n) {
        uint32_t done = 0;
        while (done < n && !pending_.empty()) {
            completeUpTo(pending_.front().fence);
            done++;
        }
        return done;
    }

    // (Futuro: o renderer de verdade consome e avança completed_fence_.)
    void completeUpTo(uint32_t f) {
        if (f > completed_fence_) completed_fence_ = f;
        while (!pending_.empty() && pending_.front().fence <= completed_fence_)
            pending_.erase(pending_.begin());
    }

    size_t channelCount() const { return channels_.size(); }
    size_t pendingCount() const { return pending_.size(); }

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
    uint32_t next_channel_ = 1;
    uint32_t next_fence_ = 1;
    uint32_t completed_fence_ = 0;
};

} // namespace hos
} // namespace mgd
