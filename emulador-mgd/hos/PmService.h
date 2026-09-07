#pragma once

// pm:dmnt (process manager) — PID do processo atual.
// cmd 1 = GetProcessId: responde u64 (1 = jogo).

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

class PmService {
public:
    PmService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            rep.cmd = 1;
            rep.payload.resize(8);
            uint64_t pid = 1;
            for (int i = 0; i < 8; i++)
                rep.payload[i] = static_cast<uint8_t>(pid >> (8 * i));
            return true;
        }
        return false;
    }
};

} // namespace hos
} // namespace mgd
