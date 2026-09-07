#pragma once

// psm (bateria) — jogo pergunta carga e carregador.
// cmd 1 = GetChargePercentage: responde 100.
// cmd 2 = IsCharging: responde 0.

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

class PsmService {
public:
    PsmService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            rep.cmd = 1;
            rep.payload = {100};
            return true;
        }
        if (req.cmd == 2) {
            rep.cmd = 1;
            rep.payload = {0};
            return true;
        }
        return false;
    }
};

} // namespace hos
} // namespace mgd
