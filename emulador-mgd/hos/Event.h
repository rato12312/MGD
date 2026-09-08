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

    uint32_t create(bool manual_reset = false, bool initially_set = false) {
        uint32_t id = next_id_++;
        flags_[id] = {initially_set, manual_reset};
        return id;
    }
    bool signal(uint32_t id) {
        auto it = flags_.find(id);
        if (it == flags_.end()) return false;
        it->second.signaled = true;
        return true;
    }
    bool clear(uint32_t id) {
        auto it = flags_.find(id);
        if (it == flags_.end()) return false;
        it->second.signaled = false;
        return true;
    }
    // Espera: true se já sinalizado (consome se auto_reset).
    // timeout_ns: 0 = infinite (stub: retorna false se não sinalizado).
    bool wait(uint32_t id, uint64_t timeout_ns = 0) {
        auto it = flags_.find(id);
        if (it == flags_.end()) return false;
        if (!it->second.signaled) return false;
        if (!it->second.manual_reset) it->second.signaled = false;
        (void)timeout_ns; // stub: sem relógio real
        return true;
    }
    bool close(uint32_t id) { return flags_.erase(id) > 0; }
    size_t count() const { return flags_.size(); }

private:
    struct Event {
        bool signaled = false;
        bool manual_reset = false;
    };
    std::unordered_map<uint32_t, Event> flags_;
    uint32_t next_id_ = 1;
};

} // namespace hos
} // namespace mgd
