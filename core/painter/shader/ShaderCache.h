#pragma once

#include "../../common/Types.h"
#include <cstdint>
#include <fstream>
#include <string>
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

    void clear() { entries_.clear(); order_.clear(); hits_ = misses_ = 0; }
    size_t size() const { return entries_.size(); }
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    void resetStats() { hits_ = misses_ = 0; }

    // Pré-compilação no loading: resolve todos os pipelines conhecidos antes
    // do primeiro frame, para nunca compilar no meio do jogo (anti-stutter).
    // resolve(key) deve ser fornecido pelo backend (ou mock nos testes).
    template <typename Resolver>
    size_t warmup(const std::vector<ShaderKey>& keys, Resolver&& resolve) {
        size_t compiled = 0;
        for (const auto& k : keys) {
            if (lookup(k)) continue; // hit: já pronto
            auto [code, hash] = resolve(k);
            store(k, code, hash);
            compiled++;
        }
        return compiled;
    }

    // Persistência perm (como FileCache): o cache sobrevive entre sessões,
    // então a segunda abertura do jogo não recompila nada.
    bool save(const std::string& path) const {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        const uint32_t magic = 0x4D475353u; // "MGSS"
        const uint32_t version = 1;
        out.write(reinterpret_cast<const char*>(&magic), 4);
        out.write(reinterpret_cast<const char*>(&version), 4);
        uint32_t n = static_cast<uint32_t>(order_.size());
        out.write(reinterpret_cast<const char*>(&n), 4);
        for (const auto& k : order_) {
            auto it = entries_.find(k);
            if (it == entries_.end() || !it->second.valid) continue;
            out.write(reinterpret_cast<const char*>(&k.asset_id), 4);
            out.write(reinterpret_cast<const char*>(&k.polygon_id), 4);
            out.write(reinterpret_cast<const char*>(&k.material_flags), 4);
            out.write(reinterpret_cast<const char*>(&it->second.pipeline_code), 4);
            out.write(reinterpret_cast<const char*>(&it->second.hash), 8);
        }
        return static_cast<bool>(out);
    }

    bool load(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;
        uint32_t magic = 0, version = 0, n = 0;
        in.read(reinterpret_cast<char*>(&magic), 4);
        in.read(reinterpret_cast<char*>(&version), 4);
        if (!in || magic != 0x4D475353u || version != 1) return false;
        in.read(reinterpret_cast<char*>(&n), 4);
        if (!in || n > 1000000u) return false;
        clear();
        for (uint32_t i = 0; i < n; ++i) {
            ShaderKey k;
            ShaderEntry e;
            in.read(reinterpret_cast<char*>(&k.asset_id), 4);
            in.read(reinterpret_cast<char*>(&k.polygon_id), 4);
            in.read(reinterpret_cast<char*>(&k.material_flags), 4);
            in.read(reinterpret_cast<char*>(&e.pipeline_code), 4);
            in.read(reinterpret_cast<char*>(&e.hash), 8);
            if (!in) { clear(); return false; }
            e.valid = true;
            entries_[k] = e;
            order_.push_back(k);
        }
        return true;
    }

    // TODO: geração real de pipeline_code no backend GPU (hoje: MGD software).
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
    std::vector<ShaderKey> order_; // FIFO: ordem de inserção
    mutable uint64_t hits_ = 0;
    mutable uint64_t misses_ = 0;
};

} // namespace shader
} // namespace mgd
