#pragma once

// lbl (backlight) — brilho da tela que o jogo lê.
// cmd 1 = GetBrightness: responde float 1.0 (bits LE).

#include <cstdint>
#include <cstring>

#include "Session.h"

namespace mgd {
namespace hos {

class LblService {
public:
    LblService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            float b = 1.0f;
            uint32_t u = 0;
            std::memcpy(&u, &b, 4);
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(u & 0xFF),
                           static_cast<uint8_t>((u >> 8) & 0xFF),
                           static_cast<uint8_t>((u >> 16) & 0xFF),
                           static_cast<uint8_t>((u >> 24) & 0xFF)};
            return true;
        }
        return false;
    }
};

} // namespace hos
} // namespace mgd
