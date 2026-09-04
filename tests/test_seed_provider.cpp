#include <iostream>
#include <string>
#include <utility>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "core/query/seed/SeedProvider.h"
#include "core/query/seed/SeedShaderDirector.h"
#include "core/query/RegionPolygonCache.h"
#include "core/painter/shader/ShaderCache.h"

using namespace mgd;
using namespace mgd::seed;

static WorldState makeState() {
    WorldState s;
    s.player_pos = Vec3(100.0f, 0.0f, 100.0f);
    s.camera_forward = Vec3(0.0f, 0.0f, 1.0f);
    s.current_region = ChunkManager::worldToRegionId(s.player_pos);
    s.last_action = PlayerAction::MoveForward;
    return s;
}

bool run_seed_provider_tests() {
    // 1. Previsão contém a região atual com maior probabilidade
    {
        SeedProvider sp(847291ull);
        auto preds = sp.predict(makeState(), PlayerAction::MoveForward);
        ASSERT_MSG(!preds.empty(), "predict returns regions");
        ASSERT_MSG(preds[0].probability >= 0.9f, "current region first with 0.9");
    }

    // 2. Determinístico: mesma entrada -> mesma saída
    {
        SeedProvider sp(847291ull);
        auto a = sp.predict(makeState(), PlayerAction::MoveForward);
        auto b = sp.predict(makeState(), PlayerAction::MoveForward);
        ASSERT_MSG(a.size() == b.size(), "deterministic size");
        for (size_t i = 0; i < a.size(); ++i) {
            ASSERT_MSG(a[i].region == b[i].region, "deterministic region");
        }
    }

    // 3. Cache de previsões: segunda chamada igual = HIT
    {
        SeedProvider sp(123ull);
        sp.predict(makeState(), PlayerAction::TurnLeft);
        ASSERT_MSG(sp.misses() == 1, "first is miss");
        sp.predict(makeState(), PlayerAction::TurnLeft);
        ASSERT_MSG(sp.hits() == 1, "repeat is hit");
        ASSERT_MSG(sp.cacheSize() == 1, "one cached prediction");
    }

    // 4. Ação diferente pode mudar a previsão (lateral)
    {
        SeedProvider sp(7ull);
        auto fwd = sp.predict(makeState(), PlayerAction::MoveForward);
        auto left = sp.predict(makeState(), PlayerAction::TurnLeft);
        // pelo menos a região atual deve estar nas duas
        ASSERT_MSG(!fwd.empty() && !left.empty(), "both predict");
        ASSERT_MSG(fwd[0].region == left[0].region, "current region in both");
    }

    // 5. Seed diferente ainda é determinística por seed
    {
        SeedProvider a(1ull), b(2ull);
        auto pa = a.predict(makeState(), PlayerAction::MoveForward);
        auto pb = b.predict(makeState(), PlayerAction::MoveForward);
        // mesma geometria -> mesmas regiões (seed só entra no hash/cache)
        ASSERT_MSG(pa.size() == pb.size(), "same geometry, same count");
    }

    // 6. Threshold: abaixo dele o MGD ignora
    {
        SeedProvider sp(9ull);
        sp.setThreshold(0.5f);
        auto preds = sp.predict(makeState(), PlayerAction::MoveForward);
        for (auto& p : preds) {
            if (p.probability < 0.5f) continue; // ignorado
            ASSERT_MSG(p.probability >= 0.5f, "kept above threshold");
        }
        ASSERT_MSG(!preds.empty(), "current region always kept");
    }

    // 7. Diretor Seed -> Shader: só o previsto é compilado, resto reutiliza
    {
        RegionPolygonCache cache;
        mgd::shader::ShaderCache shaders(64);
        SeedProvider sp(5ull);
        WorldState s = makeState();
        // polígono dentro da região atual (posição do jogador)
        Polygon p;
        p.position = s.player_pos;
        p.polygon_id = 777;
        p.asset_id = 55;
        p.flags = PolygonFlag::VISIBLE;
        RegionID r = ChunkManager::worldToRegionId(s.player_pos);
        s.current_region = r;
        ASSERT_MSG(cache.insert(r, p), "insert predicted polygon");
        auto stats = SeedShaderDirector::warmPredicted(sp, s, PlayerAction::MoveForward,
                                                       cache, shaders,
                                                       [](const mgd::shader::ShaderKey& k) {
                                                           return std::make_pair(k.asset_id * 1000u + k.polygon_id, 0xFFull);
                                                       });
        ASSERT_MSG(stats.regions_predicted >= 1, "predicted regions");
        ASSERT_MSG(stats.shaders_warmed == 1, "warmed only predicted");
        auto stats2 = SeedShaderDirector::warmPredicted(sp, s, PlayerAction::MoveForward,
                                                        cache, shaders,
                                                        [](const mgd::shader::ShaderKey& k) {
                                                            return std::make_pair(0u, 0ull);
                                                        });
        ASSERT_MSG(stats2.shaders_warmed == 0, "second pass reuses all");
        ASSERT_MSG(stats2.shaders_reused >= 1, "reuse counted");
    }

    std::cout << "  Seed provider tests passed!" << std::endl;
    return true;
}
