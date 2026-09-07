#pragma once

// vi:u (display) — esqueleto honesto: abre display, cria layer, conta presents.
// O painter vai despejar frames por aqui depois.

// cmd 1 = OpenDisplay: responde 1.
// cmd 2 = CreateLayer: responde id da layer.
// cmd 3 = Present: conta um frame apresentado + sinaliza vsync.
// cmd 4 = CreateVsyncEvent: responde id do evento.

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Event.h"
#include "Session.h"

namespace mgd {
namespace hos {

class ViService {
public:
    ViService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            rep.cmd = 1;
            return true;
        }
        if (req.cmd == 2) {
            uint32_t id = next_layer_++;
            layers_[id] = 0;
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(id & 0xFF),
                           static_cast<uint8_t>((id >> 8) & 0xFF),
                           static_cast<uint8_t>((id >> 16) & 0xFF),
                           static_cast<uint8_t>((id >> 24) & 0xFF)};
            return true;
        }
        if (req.cmd == 3) {
            if (req.payload.size() < 4) {
                rep.cmd = 0;
                return true;
            }
            uint32_t id = static_cast<uint32_t>(req.payload[0]) |
                          (static_cast<uint32_t>(req.payload[1]) << 8) |
                          (static_cast<uint32_t>(req.payload[2]) << 16) |
                          (static_cast<uint32_t>(req.payload[3]) << 24);
            auto it = layers_.find(id);
            if (it == layers_.end()) {
                rep.cmd = 0;
                return true;
            }
            it->second++;
            presented_++;
            // vsync: todo mundo esperando acorda
            for (uint32_t e : vsync_events_) events_.signal(e);
            rep.cmd = 1;
            return true;
        }
        if (req.cmd == 4) {
            uint32_t e = events_.create(false);
            vsync_events_.push_back(e);
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(e & 0xFF),
                           static_cast<uint8_t>((e >> 8) & 0xFF),
                           static_cast<uint8_t>((e >> 16) & 0xFF),
                           static_cast<uint8_t>((e >> 24) & 0xFF)};
            return true;
        }
        return false;
    }

    uint64_t presented() const { return presented_; }
    size_t layerCount() const { return layers_.size(); }
    EventTable& events() { return events_; }

private:
    std::unordered_map<uint32_t, uint64_t> layers_;
    std::vector<uint32_t> vsync_events_;
    EventTable events_;
    uint32_t next_layer_ = 1;
    uint64_t presented_ = 0;
};

} // namespace hos
} // namespace mgd
