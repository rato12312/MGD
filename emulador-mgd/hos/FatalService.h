#pragma once

// fatal:u (erros fatais) — em vez de sumir, registra o código.
// Essencial para debug: diz onde o jogo quebrou.

// cmd 1 = ThrowFatal: payload u64 código; registra e responde 1.

#include <cstdint>
#include <vector>

#include "Session.h"

namespace mgd {
namespace hos {

class FatalService {
public:
    FatalService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            uint64_t code = 0;
            for (size_t i = 0; i < req.payload.size() && i < 8; i++)
                code |= static_cast<uint64_t>(req.payload[i]) << (8 * i);
            codes_.push_back(code);
            rep.cmd = 1;
            return true;
        }
        return false;
    }

    size_t fatalCount() const { return codes_.size(); }
    uint64_t lastFatal() const { return codes_.empty() ? 0 : codes_.back(); }

private:
    std::vector<uint64_t> codes_;
};

} // namespace hos
} // namespace mgd
