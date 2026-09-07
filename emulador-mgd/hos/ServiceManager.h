#pragma once

// Serviço sm:: GetService(nome) -> handle de sessão.
// Usa PortRegistry (portas) + sessões compartilhadas.

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "PortRegistry.h"

namespace mgd {
namespace hos {

class ServiceManager {
public:
    ServiceManager() {
        ports_.registerPort("sm:");
    }

    bool publish(const std::string& name) { return ports_.registerPort(name); }

    // cmd 1 = GetService: payload = nome; out = id de sessão ou 0.
    // Retorna true se o comando foi entendido.
    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd != 1) return false;
        std::string name(req.payload.begin(), req.payload.end());
        std::shared_ptr<Session> cli, srv;
        if (!ports_.connect(name, cli, srv)) {
            rep.cmd = 0; // não achou
            return true;
        }
        uint32_t id = next_session_++;
        sessions_[id] = SessionPair{cli, srv};
        rep.cmd = 1;
        rep.payload = {static_cast<uint8_t>(id & 0xFF),
                       static_cast<uint8_t>((id >> 8) & 0xFF),
                       static_cast<uint8_t>((id >> 16) & 0xFF),
                       static_cast<uint8_t>((id >> 24) & 0xFF)};
        return true;
    }

    bool session(uint32_t id, std::shared_ptr<Session>& cli) const {
        auto it = sessions_.find(id);
        if (it == sessions_.end()) return false;
        cli = it->second.client;
        return true;
    }

    size_t sessionCount() const { return sessions_.size(); }

private:
    struct SessionPair {
        std::shared_ptr<Session> client;
        std::shared_ptr<Session> server;
    };
    PortRegistry ports_;
    std::unordered_map<uint32_t, SessionPair> sessions_;
    uint32_t next_session_ = 1;
};

} // namespace hos
} // namespace mgd
