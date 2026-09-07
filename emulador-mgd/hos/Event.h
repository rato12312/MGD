#pragma once

// Evento de sincronização: jogo espera, outro lado sinaliza.
// Single-thread: wait vira consulta (sem bloquear de verdade).
// SVC entra quando os números estiverem confirmados.

#include <cstdint>
#include <unordered_map>

namespace mgd {
namespace hos {

class EventTable {
public:
    EventTable() = default;

    uint32_t create(bool initially_set = false) {
        uint32_t id = next_id_++;
        flags_[id] = initially_set;
        return id;
    }
    bool signal(uint32_t id) {
        auto it = flags_.find(id);
        if (it == flags_.end()) return false;
        it->second = true;
        return true;
    }
    bool clear(uint32_t id) {
        auto it = flags_.find(id);
        if (it == flags_.end()) return false;
        it->second = false;
        return true;
    }
    // Espera: true se já sinalizado (consome se auto_clear).
    bool wait(uint32_t id, bool auto_clear = true) {
        auto it = flags_.find(id);
        if (it == flags_.end() || !it->second) return false;
        if (auto_clear) it->second = false;
        return true;
    }
    bool close(uint32_t id) { return flags_.erase(id) > 0; }
    size_t count() const { return flags_.size(); }

private:
    std::unordered_map<uint32_t, bool> flags_;
    uint32_t next_id_ = 1;
};

} // namespace hos
} // namespace mgd
