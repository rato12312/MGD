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
        uint32_t id = 0;
        if (!connectService(name, id)) {
            rep.cmd = 0; // não achou
            return true;
        }
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

    // Para a bomba do kernel: todas as sessões com o nome do serviço.
    std::vector<std::pair<uint32_t, std::shared_ptr<Session>>> allSessions() const {
        std::vector<std::pair<uint32_t, std::shared_ptr<Session>>> out;
        for (const auto& kv : sessions_) out.push_back({kv.first, kv.second.server});
        return out;
    }
    std::string serviceOf(uint32_t id) const {
        auto it = sessions_.find(id);
        return it == sessions_.end() ? "" : it->second.service;
    }

    size_t sessionCount() const { return sessions_.size(); }

private:
    struct SessionPair {
        std::shared_ptr<Session> client;
        std::shared_ptr<Session> server;
        std::string service;
    };
    PortRegistry ports_;
    std::unordered_map<uint32_t, SessionPair> sessions_;
    uint32_t next_session_ = 1;

    // Registra sessão sabendo o serviço (usado pelo dispatch).
    bool connectService(const std::string& name, uint32_t& idOut) {
        std::shared_ptr<Session> cli, srv;
        if (!ports_.connect(name, cli, srv)) return false;
        idOut = next_session_++;
        sessions_[idOut] = SessionPair{cli, srv, name};
        return true;
    }

} // namespace hos
} // namespace mgd
