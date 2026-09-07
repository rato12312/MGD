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
    // outro cmd = não entendido (false), ioctl futuro.
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
            uint32_t id = static_cast<uint32_t>(req.payload[0]) |
                          (static_cast<uint32_t>(req.payload[1]) << 8) |
                          (static_cast<uint32_t>(req.payload[2]) << 16) |
                          (static_cast<uint32_t>(req.payload[3]) << 24);
            rep.cmd = channels_.erase(id) > 0 ? 1 : 0;
            return true;
        }
        return false;
    }

    size_t channelCount() const { return channels_.size(); }

private:
    std::unordered_map<uint32_t, uint32_t> channels_;
    uint32_t next_channel_ = 1;
};

} // namespace hos
} // namespace mgd
