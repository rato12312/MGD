#pragma once

#include "EmulatorHandoff.h"
#include "../query/RegionPolygonCache.h"
#include "../query/PolygonConsultant.h"
#include "../query/dna/DnaPipeline.h"
#include "../mental_map/ChunkManager.h"
#include "../mental_map/MentalMap.h"
#include <cstdint>
#include <vector>

namespace mgd {
namespace bridge {

// Runtime: tudo pelo Mapa Mental. A cada frame:
// 1. HandoffFrame chega (captura do jogo).
// 2. Polígonos vão para o cache por região.
// 3. Consultador alimenta o Mapa Mental (só referências).
// 4. Pipeline DNA pinta com incremental.
// Nada é renderizado de verdade: só lookup + blit do que mudou.
struct RuntimeFrameStats {
    uint32_t polygons_fed = 0;
    uint32_t rebuilt = 0;
    uint32_t reused = 0;
    uint32_t pixels_written = 0;
};

class MentalMapRuntime {
public:
    MentalMapRuntime() : consultant_(&cache_, nullptr) {}
    MentalMapRuntime(int fbW, int fbH) : consultant_(&cache_, nullptr), pipe_(fbW, fbH) {}

    void resize(int fbW, int fbH) { pipe_.resize(fbW, fbH); }

    RuntimeFrameStats step(const HandoffFrame& frame,
                           const std::vector<Polygon>& polys) {
        RuntimeFrameStats stats;
        // 1-2. Cache por região a partir do handoff.
        for (const auto& p : polys) {
            RegionID r = ChunkManager::worldToRegionId(p.position);
            cache_.insert(r, p);
        }
        // 3. Mapa Mental (só referências, sem copiar assets).
        for (PolygonID pid : frame.visible_polygons) {
            if (consultant_.feedMentalMap(map_, pid) != INVALID_ENTITY_ID) {
                stats.polygons_fed++;
            }
        }
        // 4. Pipeline: primeira vez calcula, depois só o que mudou.
        // (Detecção fina de mudança por frame fica no ChangeDetector do pipe.)
        if (!built_) {
            pipe_.buildFromPolygons(polys, 10);
            built_ = true;
        }
        std::vector<PolygonID> empty;
        DnaFrameStats fs = pipe_.renderFrame(empty);
        stats.rebuilt = fs.rebuilt_polys;
        stats.reused = fs.reused_polys;
        stats.pixels_written = fs.pixels_written;
        return stats;
    }

    MentalMap& map() { return map_; }
    RegionPolygonCache& cache() { return cache_; }

private:
    RegionPolygonCache cache_;
    PolygonConsultant consultant_;
    MentalMap map_;
    dna::DnaPipeline pipe_{64, 32};
    bool built_ = false;
};

} // namespace bridge
} // namespace mgd
