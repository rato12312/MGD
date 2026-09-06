#pragma once

#include "Polygon.h"
#include "../common/Types.h"
#include "../common/AABB.h"
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <vector>
#include <optional>
#include <cstdint>

namespace mgd {

// CACHE ESPACIAL — Region -> Polígonos -> Asset
// Evita busca global: consulta só polígonos da região relevante.
// Não duplica grandes estruturas (armazena Polygon por valor em vetor contíguo
// por região, boa localidade). Sem alocação por consulta (retorna span/view).
// Integrado ao cache existente via stats e futura persistência TODO.

class RegionPolygonCache {
public:
    RegionPolygonCache() = default;

    // Inserir polígono em região. O(1) amortizado.
    // Permite posições iguais com PolygonIDs diferentes (vetor, não mapa por posição).
    bool insert(RegionID region, const Polygon& poly);
    bool insert(RegionID region, Polygon&& poly);

    // Consulta: região -> polígonos (sem cópia, sem alocação)
    const std::vector<Polygon>& getPolygons(RegionID region) const;
    std::vector<Polygon>& getPolygons(RegionID region);

    // Encontrar PolygonID -> Polygon (O(1) via índice)
    std::optional<Polygon> findPolygon(PolygonID pid) const;
    const Polygon* findPolygonPtr(PolygonID pid) const;

    // Resolver PolygonID -> AssetID (referência, sem copiar asset)
    std::optional<AssetID> getAssetId(PolygonID pid) const;

    // Remoção e limpeza
    bool removePolygon(PolygonID pid);
    void clearRegion(RegionID region);
    void clear();

    // Orçamento de RAM: teto de polígonos; estourou, a região mais
    // antiga cai (FIFO por região). 0 = sem teto.
    void setBudget(size_t maxPolygons) { budget_ = maxPolygons; enforceBudget(); }
    size_t budget() const { return budget_; }
    uint64_t evictions() const { return evictions_; }

    // Stats para benchmark
    size_t regionCount() const;
    size_t polygonCount() const;
    size_t polygonCount(RegionID region) const;
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    void resetStats() { hits_ = 0; misses_ = 0; }

    // TODO: integração com ICache/FileCache para persistência
    // bool saveCheckpoint(const std::string& path) const;
    // bool loadCheckpoint(const std::string& path);

private:
    // Região -> lista contígua de polígonos (boa localidade)
    std::unordered_map<RegionID, std::vector<Polygon>> region_to_polys_;

    // Índice global PolygonID -> (RegionID, index no vetor) para O(1) lookup
    struct Loc { RegionID region; size_t idx; };
    std::unordered_map<PolygonID, Loc> polygon_index_;

    void enforceBudget();

    mutable uint64_t hits_ = 0;
    mutable uint64_t misses_ = 0;
    size_t budget_ = 0; // 0 = sem teto
    uint64_t evictions_ = 0;
    std::deque<RegionID> region_order_; // FIFO de regiões (mais antiga na frente)
    std::unordered_set<RegionID> region_seen_;

    static const std::vector<Polygon> kEmpty;
};

} // namespace mgd
