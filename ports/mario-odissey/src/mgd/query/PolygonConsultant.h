#pragma once

#include "RegionPolygonCache.h"
#include "Polygon.h"
#include "../mental_map/ChunkManager.h"
#include "../mental_map/MentalMap.h"
#include "../scanner/AssetRegistry.h"
#include "../common/Vec3.h"
#include <vector>
#include <optional>

namespace mgd {

// Consultador de cena por POLÍGONOS — API otimizada para consultas frequentes.
// Fluxo: posição → região (ChunkManager) → polígonos da região → PolygonID → AssetID → asset → Mapa Mental
// Reutiliza ChunkManager, RegionPolygonCache, AssetRegistry e MentalMap existentes.
// Não varre todos os assets, não relê arquivos se já em cache, não duplica estruturas grandes.

class PolygonConsultant {
public:
    PolygonConsultant() = default;
    explicit PolygonConsultant(RegionPolygonCache* cache, AssetRegistry* registry = nullptr)
        : cache_(cache), registry_(registry) {}

    void setCache(RegionPolygonCache* c) { cache_ = c; }
    void setRegistry(AssetRegistry* r) { registry_ = r; }

    // Consulta principal: posição -> região -> polígonos (sem cópia, sem alocação por consulta)
    const std::vector<Polygon>& queryRegion(RegionID region) const;
    const std::vector<Polygon>& queryByPosition(const Vec3& pos) const;

    // Encontrar PolygonID
    std::optional<Polygon> findPolygon(PolygonID pid) const;
    const Polygon* findPolygonPtr(PolygonID pid) const;

    // Resolver PolygonID -> AssetID -> asset (referência, sem copiar asset inteiro)
    std::optional<AssetID> resolveAssetId(PolygonID pid) const;
    std::optional<RegisteredAsset> resolveAsset(PolygonID pid) const;
    std::optional<RegisteredAsset> resolveAssetById(AssetID aid) const;

    // Inserir polígono (posição + PolygonID + AssetID + flags)
    bool insertPolygon(RegionID region, const Polygon& poly);
    bool insertPolygon(const Vec3& position, RegionID region, PolygonID pid, AssetID aid, uint32_t flags = 0);

    // Alimentar Mapa Mental sem duplicar assets (só referências/estado)
    // Cria MentalEntity leve com polygon_id/asset_id/position/region
    EntityID feedMentalMap(MentalMap& map, PolygonID pid) const;
    size_t feedMentalMapRegion(MentalMap& map, RegionID region) const;

    // LOD refinado: classifica por distância/área projetada (não artificial)
    struct ScoredPolygon {
        const Polygon* poly = nullptr;
        float distance = 0.0f;
        float screenArea = 0.0f; // estimado via distance e bounds
        uint32_t flags = 0;
    };
    // Consulta com score para LOD: retorna polígonos da região ordenados por distância,
    // já filtrando por screenArea mínima (pedra gigante longe = quadradinho barato)
    std::vector<ScoredPolygon> queryScored(const Vec3& cameraPos, RegionID region, float minScreenArea = 1.0f) const;
    std::vector<ScoredPolygon> queryScoredByPosition(const Vec3& cameraPos, const Vec3& queryPos, float minScreenArea = 1.0f) const;

    // Stats
    uint64_t hits() const;
    uint64_t misses() const;
    void resetStats() const;

private:
    RegionPolygonCache* cache_ = nullptr;
    AssetRegistry* registry_ = nullptr;
};

} // namespace mgd
