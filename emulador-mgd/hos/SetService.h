#pragma once

// set:sys (settings) — idioma e região que o jogo lê no boot.
// cmd 1 = GetLanguageCode: responde u64 ("en-US").
// cmd 2 = GetRegionCode: responde u32 (1 = Américas).

#include <cstdint>
#include <cstring>

#include "Session.h"

namespace mgd {
namespace hos {

class SetService {
public:
    SetService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            uint64_t code = 0;
            std::memcpy(&code, "en-US", 5);
            rep.cmd = 1;
            rep.payload.resize(8);
            for (int i = 0; i < 8; i++)
                rep.payload[i] = static_cast<uint8_t>(code >> (8 * i));
            return true;
        }
        if (req.cmd == 2) {
            rep.cmd = 1;
            rep.payload = {1, 0, 0, 0};
            return true;
        }
        return false;
    }
};

} // namespace hos
} // namespace mgd
