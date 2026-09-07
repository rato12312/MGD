#pragma once

// apm (performance) — modo do aparelho que o jogo consulta.
// cmd 1 = GetPerformanceMode: responde 0 (handheld) ou 1 (docked).
// cmd 2 = SetPerformanceMode: grava (stub honesto, sem efeito real).

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

class ApmService {
public:
    ApmService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(docked_ ? 1 : 0)};
            return true;
        }
        if (req.cmd == 2) {
            if (!req.payload.empty()) docked_ = req.payload[0] != 0;
            rep.cmd = 1;
            return true;
        }
        return false;
    }

    bool docked() const { return docked_; }

private:
    bool docked_ = false; // MGD Edge: sempre handheld (barato)
};

} // namespace hos
} // namespace mgd
