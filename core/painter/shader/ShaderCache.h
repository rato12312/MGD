#pragma once

#include "../../common/Types.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mgd {
namespace shader {

// Cache de shaders/pipelines por base do polígono.
// Chave = AssetID + PolygonID + flags de material. Shader já resolvido
// para aquele asset nunca recompila: lookup O(1), hit reutiliza.
// Filosofia MGD: descobrir uma vez, indexar e reutilizar enquanto válido.
struct ShaderKey {
    AssetID asset_id = INVALID_ASSET_ID;
    PolygonID polygon_id = INVALID_POLYGON_ID;
    uint32_t material_flags = 0;

    bool operator==(const ShaderKey& o) const {
        return asset_id == o.asset_id && polygon_id == o.polygon_id &&
               material_flags == o.material_flags;
    }
};

struct ShaderKeyHash {
    size_t operator()(const ShaderKey& k) const noexcept {
        size_t h = static_cast<size_t>(k.asset_id) * 73856093u;
        h ^= static_cast<size_t>(k.polygon_id) * 19349663u;
        h ^= static_cast<size_t>(k.material_flags) * 83492791u;
        return h;
    }
};

struct ShaderEntry {
    uint32_t pipeline_code = 0; // código do pipeline resolvido (backend real gera)
    uint64_t hash = 0;          // hash do fonte para invalidar
    bool valid = false;
};

class ShaderCache {
public:
    explicit ShaderCache(size_t capacity = 4096) : capacity_(capacity) {}

    // Hit: retorna pipeline pronto. Miss: nullopt (chamador compila e insere).
    const ShaderEntry* lookup(const ShaderKey& key) const {
        auto it = entries_.find(key);
        if (it == entries_.end() || !it->second.valid) {
            misses_++;
            return nullptr;
        }
        hits_++;
        return &it->second;
    }

    void store(const ShaderKey& key, uint32_t pipeline_code, uint64_t hash) {
        if (entries_.find(key) == entries_.end()) {
            if (entries_.size() >= capacity_) evictOne();
            order_.push_back(key);
        }
        ShaderEntry e;
        e.pipeline_code = pipeline_code;
        e.hash = hash;
        e.valid = true;
        entries_[key] = e;
    }

    bool invalidateAsset(AssetID aid) {
        bool removed = false;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->first.asset_id == aid) {
                it = entries_.erase(it);
                removed = true;
            } else {
                ++it;
            }
        }
        return removed;
    }

    void clear() { entries_.clear(); hits_ = misses_ = 0; }
    size_t size() const { return entries_.size(); }
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    void resetStats() { hits_ = misses_ = 0; }

    // TODO: persistência em cache/shader/ (perm, como FileCache) e
    // geração real de pipeline_code no backend GPU (hoje: MGD software).
    // TODO: LRU por frame quando ligar no renderer real (hoje: FIFO).

private:
    void evictOne() {
        if (!order_.empty()) {
            entries_.erase(order_.front());
            order_.erase(order_.begin());
        } else if (!entries_.empty()) {
            entries_.erase(entries_.begin());
        }
    }

    size_t capacity_;
    std::unordered_map<ShaderKey, ShaderEntry, ShaderKeyHash> entries_;
    std::vector<ShaderKey> order_; // TODO: manter em store() para FIFO real
    mutable uint64_t hits_ = 0;
    mutable uint64_t misses_ = 0;
};

} // namespace shader
} // namespace mgd
