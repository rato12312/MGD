#pragma once

#include "../../common/Types.h"
#include <cstdint>
#include <unordered_map>

namespace mgd {
namespace dna {

// Detecção de mudanças: Mundo -> Objeto -> Polígono.
// NÃO mudou -> reutiliza; mudou -> desce um nível. Só recalcula o polígono
// que mudou, reutilizando DNA, mapa de pixels e cache do resto.
class ChangeDetector {
public:
    void beginFrame() {}
    void touchWorld() { world_version_++; }
    void touchObject(uint32_t objectId) { object_ver_[objectId] = ++object_seq_; }
    void touchPolygon(PolygonID pid) { poly_ver_[pid] = ++poly_seq_; }

    // Retorna true se o polígono precisa ser recalculado desde lastSeen.
    bool polygonChanged(PolygonID pid, uint64_t lastSeenWorld, uint64_t lastSeenPoly) const {
        if (lastSeenWorld != world_version_) return true;
        auto it = poly_ver_.find(pid);
        if (it == poly_ver_.end()) return true; // novo -> calcula uma vez
        return it->second > lastSeenPoly;
    }

    uint64_t worldVersion() const { return world_version_; }
    uint64_t polygonVersion(PolygonID pid) const {
        auto it = poly_ver_.find(pid);
        return it == poly_ver_.end() ? 0 : it->second;
    }

private:
    uint64_t world_version_ = 1;
    uint64_t object_seq_ = 1;
    uint64_t poly_seq_ = 1;
    std::unordered_map<uint32_t, uint64_t> object_ver_;
    std::unordered_map<PolygonID, uint64_t> poly_ver_;
};

} // namespace dna
} // namespace mgd
