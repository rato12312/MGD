#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace port {

// Cache de views do port, baseado no padrão do Eden
// (`image_views.try_emplace(descriptor)`): a view é identificada pelo
// descritor (imagem + formato + faixa) e criada uma única vez.
// Na Mali, recriar view mutável é caro — aqui nunca se recria.
struct ViewDescriptor {
    uint64_t image_id = 0;
    uint32_t format = 0;      // código do formato (ex.: RGBA8 photography)
    uint32_t base_layer = 0;
    uint32_t layer_count = 1;

    bool operator==(const ViewDescriptor& o) const {
        return image_id == o.image_id && format == o.format &&
               base_layer == o.base_layer && layer_count == o.layer_count;
    }
};

struct ViewDescriptorHash {
    size_t operator()(const ViewDescriptor& d) const noexcept {
        size_t h = static_cast<size_t>(d.image_id) * 73856093u;
        h ^= static_cast<size_t>(d.format) * 19349663u;
        h ^= static_cast<size_t>(d.base_layer) * 83492791u;
        h ^= static_cast<size_t>(d.layer_count) * 2246822519u;
        return h;
    }
};

class ViewCache {
public:
    // Retorna o id da view (cria uma única vez por descritor).
    // Segunda chamada com mesmo descritor = HIT, sem recriar.
    uint32_t getOrCreate(const ViewDescriptor& desc) {
        auto it = views_.find(desc);
        if (it != views_.end()) {
            hits_++;
            return it->second;
        }
        misses_++;
        uint32_t id = next_id_++;
        views_[desc] = id;
        return id;
    }

    void clear() { views_.clear(); next_id_ = 1; hits_ = misses_ = 0; }
    size_t size() const { return views_.size(); }
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    void resetStats() { hits_ = misses_ = 0; }

private:
    std::unordered_map<ViewDescriptor, uint32_t, ViewDescriptorHash> views_;
    uint32_t next_id_ = 1;
    mutable uint64_t hits_ = 0;
    mutable uint64_t misses_ = 0;
};

} // namespace port
