#pragma once

// Registro de portas: nome ("sm:", "nvdrv:a"...) -> fila de sessões.
// Servidor registra, cliente conecta e ganha um par ligado.

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "Session.h"

namespace mgd {
namespace hos {

class PortRegistry {
public:
    bool registerPort(const std::string& name, uint32_t max_sessions = 8) {
        if (ports_.find(name) != ports_.end()) return false;
        ports_[name] = {max_sessions, 0};
        return true;
    }
    bool hasPort(const std::string& name) const {
        return ports_.find(name) != ports_.end();
    }
    // Conecta: devolve par (cliente, servidor) já ligado.
    bool connect(const std::string& name,
                 std::shared_ptr<Session>& client,
                 std::shared_ptr<Session>& server) {
        auto it = ports_.find(name);
        if (it == ports_.end()) return false;
        if (it->second.current_sessions >= it->second.max_sessions) return false;
        
        // Cria nova sessão para este cliente
        auto s = std::make_shared<Session>();
        client = s;
        server = s; // Modelo simplificado: mesma sessão para ambos
        it->second.current_sessions++;
        return true;
    }
    void disconnect(const std::string& name) {
        auto it = ports_.find(name);
        if (it != ports_.end() && it->second.current_sessions > 0) {
            it->second.current_sessions--;
        }
    }
    size_t portCount() const { return ports_.size(); }
    uint32_t getMaxSessions(const std::string& name) const {
        auto it = ports_.find(name);
        return it != ports_.end() ? it->second.max_sessions : 0;
    }

private:
    struct PortInfo {
        uint32_t max_sessions = 8;
        uint32_t current_sessions = 0;
    };
    std::unordered_map<std::string, PortInfo> ports_;
};

} // namespace hos
} // namespace mgd
