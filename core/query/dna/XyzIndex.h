#pragma once

#include "../../common/Vec3.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mgd {
namespace dna {

// Índices X/Y/Z: em vez de guardar 3 floats por polígono e comparar como
// informação independente, quantiza a posição e o DNA aponta para o índice.
// Menos memória + menos buscas + melhor cache.
class XyzIndex {
public:
    explicit XyzIndex(float step = 0.25f) : step_(step) {}

    uint32_t intern(const Vec3& p) {
        int32_t ix = static_cast<int32_t>(p.x / step_);
        int32_t iy = static_cast<int32_t>(p.y / step_);
        int32_t iz = static_cast<int32_t>(p.z / step_);
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(ix)) << 32) ^
                       (static_cast<uint64_t>(static_cast<uint32_t>(iy)) * 73856093ull) ^
                       (static_cast<uint64_t>(static_cast<uint32_t>(iz)) * 19349663ull);
        auto it = id_by_key_.find(key);
        if (it != id_by_key_.end()) return it->second;
        uint32_t id = static_cast<uint32_t>(positions_.size()) + 1; // 0 = inválido
        positions_.push_back(Vec3(ix * step_, iy * step_, iz * step_));
        id_by_key_[key] = id;
        return id;
    }

    Vec3 decode(uint32_t id) const {
        if (id == 0 || id > positions_.size()) return Vec3{};
        return positions_[id - 1];
    }

    size_t size() const { return positions_.size(); }
    void clear() { positions_.clear(); id_by_key_.clear(); }

private:
    float step_;
    std::vector<Vec3> positions_;
    std::unordered_map<uint64_t, uint32_t> id_by_key_;
};

} // namespace dna
} // namespace mgd
