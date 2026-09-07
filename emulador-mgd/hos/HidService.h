#pragma once

// hid:u (input) — botões como bitmask que o jogo lê.
// O aparelho real escreve aqui; o jogo só consulta.

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

// Botões do Switch (resumo honesto).
enum HidButton : uint64_t {
    BTN_A = 1ull << 0,
    BTN_B = 1ull << 1,
    BTN_X = 1ull << 2,
    BTN_Y = 1ull << 3,
    BTN_PLUS = 1ull << 9,
    BTN_MINUS = 1ull << 8,
};

// cmd 1 = Press: payload u64 OR.
// cmd 2 = Release: payload u64 AND NOT.
// cmd 3 = GetState: responde u64 mask.

class HidService {
public:
    HidService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1 || req.cmd == 2) {
            if (req.payload.size() < 8) {
                rep.cmd = 0;
                return true;
            }
            uint64_t m = 0;
            for (int i = 0; i < 8; i++) m |= static_cast<uint64_t>(req.payload[i]) << (8 * i);
            if (req.cmd == 1) buttons_ |= m;
            else buttons_ &= ~m;
            rep.cmd = 1;
            return true;
        }
        if (req.cmd == 3) {
            rep.cmd = 1;
            rep.payload.resize(8);
            for (int i = 0; i < 8; i++)
                rep.payload[i] = static_cast<uint8_t>(buttons_ >> (8 * i));
            return true;
        }
        return false;
    }

    uint64_t buttons() const { return buttons_; }

private:
    uint64_t buttons_ = 0;
};

} // namespace hos
} // namespace mgd
