#pragma once

// time:u (relógio) — hora Unix que o jogo consulta.
// cmd 1 = GetCurrentTime: responde u64.
// cmd 2 = SetCurrentTime: payload u64 grava.

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

class TimeService {
public:
    explicit TimeService(uint64_t start = 1700000000ull) : now_(start) {}

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            rep.cmd = 1;
            rep.payload.resize(8);
            for (int i = 0; i < 8; i++)
                rep.payload[i] = static_cast<uint8_t>(now_ >> (8 * i));
            return true;
        }
        if (req.cmd == 2) {
            if (req.payload.size() < 8) {
                rep.cmd = 0;
                return true;
            }
            now_ = 0;
            for (int i = 0; i < 8; i++)
                now_ |= static_cast<uint64_t>(req.payload[i]) << (8 * i);
            rep.cmd = 1;
            return true;
        }
        return false;
    }

    uint64_t now() const { return now_; }

private:
    uint64_t now_;
};

} // namespace hos
} // namespace mgd
