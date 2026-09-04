#pragma once

#include "SeedProvider.h"
#include "../RegionPolygonCache.h"
#include "../../painter/shader/ShaderCache.h"
#include <cstdint>
#include <vector>

namespace mgd {
namespace seed {

// Diretor Seed -> Shader: o Seed Provider calcula O QUE se mexe (regiões
// prováveis), e o shader muda SÓ aquilo. Parado reutiliza do ShaderCache.
// Inclui tudo: previsão + warmup direcionado + stats.
struct SeedShaderStats {
    size_t regions_predicted = 0;
    size_t shaders_warmed = 0;   // compilados agora (miss)
    size_t shaders_reused = 0;   // já estavam prontos (hit)
};

class SeedShaderDirector {
public:
    // Para cada região prevista acima do threshold: coleta os AssetIDs dos
    // polígonos e faz warmup dos shaders só deles. Retorna o que fez.
    // resolve(key) vem do backend (ou mock): (pipeline_code, hash).
    template <typename Resolver>
    static SeedShaderStats warmPredicted(SeedProvider& provider,
                                         const WorldState& state,
                                         PlayerAction action,
                                         RegionPolygonCache& cache,
                                         shader::ShaderCache& shaders,
                                         Resolver&& resolve) {
        SeedShaderStats stats;
        auto preds = provider.predict(state, action);
        for (const auto& p : preds) {
            if (p.probability < provider.threshold()) continue;
            stats.regions_predicted++;
            const auto& polys = cache.getPolygons(p.region);
            for (const auto& poly : polys) {
                shader::ShaderKey key{poly.asset_id, poly.polygon_id, 0};
                if (shaders.lookup(key)) { stats.shaders_reused++; continue; }
                auto [code, hash] = resolve(key);
                shaders.store(key, code, hash);
                stats.shaders_warmed++;
            }
        }
        return stats;
    }
};

} // namespace seed
} // namespace mgd
