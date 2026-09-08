#pragma once

// appletOE (applet manager) — porta dos applets do sistema.
// cmd 1 = GetAppletResourceUserId: responde 1 (self).

#include <cstdint>

#include "Session.h"

namespace mgd {
namespace hos {

class AppletService {
public:
    AppletService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            rep.cmd = 1;
            rep.payload = {1, 0, 0, 0, 0, 0, 0, 0};
            return true;
        }
        return false;
    }
};

} // namespace hos
} // namespace mgd
