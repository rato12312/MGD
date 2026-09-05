#pragma once

#include "SeedProvider.h"
#include "../RegionPolygonCache.h"
#include "../dna/PixelMap.h"
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

    // Filtro genérico: shader só onde aparece coisa (visibilidade/LOD/cena).
    // O predicado decide por polígono (ex.: dentro do frustum, LOD visível);
    // o resto nem é tocado. Raio é só o caso padrão.
    template <typename Resolver, typename Predicate>
    static SeedShaderStats warmFiltered(SeedProvider& provider,
                                        const WorldState& state,
                                        PlayerAction action,
                                        RegionPolygonCache& cache,
                                        shader::ShaderCache& shaders,
                                        Resolver&& resolve,
                                        Predicate&& visible) {
        SeedShaderStats stats;
        auto preds = provider.predict(state, action);
        for (const auto& p : preds) {
            if (p.probability < provider.threshold()) continue;
            stats.regions_predicted++;
            const auto& polys = cache.getPolygons(p.region);
            for (const auto& poly : polys) {
                if (!visible(poly)) continue; // fora da cena visível: pula
                shader::ShaderKey key{poly.asset_id, poly.polygon_id, 0};
                if (shaders.lookup(key)) { stats.shaders_reused++; continue; }
                auto [code, hash] = resolve(key);
                shaders.store(key, code, hash);
                stats.shaders_warmed++;
            }
        }
        return stats;
    }

    // Novo: shader calcula o POLÍGONO para onde vai, não o asset.
    // Na hora que o Seed pensa em ir para a posição X, os polígonos num
    // raio dela já são selecionados e seus shaders aquecidos. O resto
    // nem é tocado. O LOD vem da distância (shader simples longe, full perto).
    template <typename Resolver>
    static SeedShaderStats warmPredictedPositions(SeedProvider& provider,
                                                  const WorldState& state,
                                                  PlayerAction action,
                                                  RegionPolygonCache& cache,
                                                  shader::ShaderCache& shaders,
                                                  Resolver&& resolve,
                                                  float radius = 4096.0f,
                                                  float lodNear = 20.0f, float lodFar = 60.0f) {
        SeedShaderStats stats;
        auto preds = provider.predict(state, action);
        float r2 = radius * radius;
        for (const auto& p : preds) {
            if (p.probability < provider.threshold()) continue;
            stats.regions_predicted++;
            const auto& polys = cache.getPolygons(p.region);
            for (const auto& poly : polys) {
                Vec3 d = poly.position - state.player_pos;
                if (d.lengthSq() > r2) continue; // fora do alcance pensado
                float dist = d.length();
                uint8_t lod = static_cast<uint8_t>(dna::PixelMap::selectLod(dist, lodNear, lodFar));
                shader::ShaderKey key{poly.asset_id, poly.polygon_id, 0, lod};
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
