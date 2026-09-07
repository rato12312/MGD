#pragma once

// audren:u (áudio render) — esqueleto honesto: abre sessão,
// start/stop, conta buffers. Som de verdade vem depois.

// cmd 1 = Open: responde 1.
// cmd 2 = Start: responde 1.
// cmd 3 = Stop: responde 1.
// cmd 4 = QueueBuffer: soma bytes enfileirados, responde 1.

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

class AudService {
public:
    AudService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1 || req.cmd == 2 || req.cmd == 3) {
            if (req.cmd == 2) started_ = true;
            if (req.cmd == 3) started_ = false;
            rep.cmd = 1;
            return true;
        }
        if (req.cmd == 4) {
            queued_bytes_ += req.payload.size();
            rep.cmd = 1;
            return true;
        }
        return false;
    }

    bool started() const { return started_; }
    uint64_t queuedBytes() const { return queued_bytes_; }

private:
    bool started_ = false;
    uint64_t queued_bytes_ = 0;
};

} // namespace hos
} // namespace mgd
