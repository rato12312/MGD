#pragma once

// Mutex: dono trava, outro espera, dono destrava.
// Single-thread honesto: rastreia dono por id de thread.

#include <cstdint>
#include <unordered_map>

namespace mgd {
namespace hos {

class MutexTable {
public:
    MutexTable() = default;

    uint32_t create() {
        uint32_t id = next_id_++;
        owners_[id] = 0; // 0 = livre
        return id;
    }
    // Trava para owner. true = conseguiu (livre ou já dono).
    bool lock(uint32_t id, uint64_t owner) {
        auto it = owners_.find(id);
        if (it == owners_.end()) return false;
        if (it->second != 0 && it->second != owner) return false;
        it->second = owner;
        return true;
    }
    // Destrava: só o dono. false = não é dono ou inexistente.
    bool unlock(uint32_t id, uint64_t owner) {
        auto it = owners_.find(id);
        if (it == owners_.end() || it->second != owner) return false;
        it->second = 0;
        return true;
    }
    bool close(uint32_t id) { return owners_.erase(id) > 0; }

private:
    std::unordered_map<uint32_t, uint64_t> owners_;
    uint32_t next_id_ = 1;
};

} // namespace hos
} // namespace mgd
