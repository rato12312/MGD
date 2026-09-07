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
    bool registerPort(const std::string& name) {
        if (ports_.find(name) != ports_.end()) return false;
        ports_[name] = 1;
        return true;
    }
    bool hasPort(const std::string& name) const {
        return ports_.find(name) != ports_.end();
    }
    // Conecta: devolve par (cliente, servidor) já ligado.
    bool connect(const std::string& name,
                 std::shared_ptr<Session>& client,
                 std::shared_ptr<Session>& server) {
        if (!hasPort(name)) return false;
        // Sessão compartilhada: os dois lados operam na mesma fila.
        // (Modelo simplificado honesto: 1 sessão, 2 pontas.)
        auto s = std::make_shared<Session>();
        client = s;
        server = s;
        return true;
    }
    size_t portCount() const { return ports_.size(); }

private:
    std::unordered_map<std::string, int> ports_;
};

} // namespace hos
} // namespace mgd
