#pragma once

// acc:u (conta) — jogo pergunta quantos usuários.
// cmd 1 = GetUserCount: responde 1.

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

class AccService {
public:
    AccService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            rep.cmd = 1;
            rep.payload = {1, 0, 0, 0};
            return true;
        }
        return false;
    }
};

} // namespace hos
} // namespace mgd
