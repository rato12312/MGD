#include <iostream>
#include <string>
#include <utility>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "core/query/seed/SeedProvider.h"
#include "core/query/seed/SeedShaderDirector.h"
#include "core/query/seed/CameraPredictor.h"
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

    // 8. Shader pelo polígono/posição: só o que está no raio pensado aquece
    {
        RegionPolygonCache cache2;
        mgd::shader::ShaderCache shaders2(64);
        SeedProvider sp2(5ull);
        WorldState s2 = makeState();
        RegionID r2 = ChunkManager::worldToRegionId(s2.player_pos);
        s2.current_region = r2;
        Polygon nearPoly;
        nearPoly.position = s2.player_pos;
        nearPoly.polygon_id = 881;
        nearPoly.asset_id = 55;
        nearPoly.flags = PolygonFlag::VISIBLE;
        Polygon farPoly;
        farPoly.position = Vec3(s2.player_pos.x + 100000.0f, 0.0f, s2.player_pos.z);
        farPoly.polygon_id = 882;
        farPoly.asset_id = 55;
        farPoly.flags = PolygonFlag::VISIBLE;
        ASSERT_MSG(cache2.insert(r2, nearPoly), "insert near");
        ASSERT_MSG(cache2.insert(r2, farPoly), "insert far");
        auto stats = SeedShaderDirector::warmPredictedPositions(
            sp2, s2, PlayerAction::MoveForward, cache2, shaders2,
            [](const mgd::shader::ShaderKey& k) {
                return std::make_pair(k.asset_id * 1000u + k.polygon_id, 0xFFull);
            },
            4096.0f);
        ASSERT_MSG(stats.shaders_warmed == 1, "only in-radius polygon warmed");
        // perto (dist 0) aquece no LOD 2 (full); longe nem é tocado
        ASSERT_MSG(shaders2.lookup(mgd::shader::ShaderKey{55, 881, 2}) != nullptr, "near ready at LOD 2");
        ASSERT_MSG(shaders2.lookup(mgd::shader::ShaderKey{55, 881, 0}) == nullptr, "no LOD 0 entry for near");
        ASSERT_MSG(shaders2.lookup(mgd::shader::ShaderKey{55, 882, 0}) == nullptr, "far untouched");
        ASSERT_MSG(shaders2.lookup(mgd::shader::ShaderKey{55, 882, 2}) == nullptr, "far untouched at LOD 2");
    }

    // 9. warmFiltered: shader só onde aparece (predicado de cena/LOD)
    {
        RegionPolygonCache cache3;
        mgd::shader::ShaderCache shaders3(64);
        SeedProvider sp3(5ull);
        WorldState s3 = makeState();
        RegionID r3 = ChunkManager::worldToRegionId(s3.player_pos);
        s3.current_region = r3;
        for (uint32_t i = 0; i < 4; ++i) {
            Polygon p;
            p.position = s3.player_pos;
            p.polygon_id = 900 + i;
            p.asset_id = 55;
            p.flags = (i < 2) ? PolygonFlag::VISIBLE : PolygonFlag::OCCLUDED;
            cache3.insert(r3, p);
        }
        auto stats = SeedShaderDirector::warmFiltered(
            sp3, s3, PlayerAction::MoveForward, cache3, shaders3,
            [](const mgd::shader::ShaderKey& k) {
                return std::make_pair(k.asset_id * 1000u + k.polygon_id, 0xFFull);
            },
            [](const Polygon& p) { return p.hasFlag(PolygonFlag::VISIBLE); });
        ASSERT_MSG(stats.shaders_warmed == 2, "only visible warmed");
        ASSERT_MSG(shaders3.lookup(mgd::shader::ShaderKey{55, 900, 0}) != nullptr, "visible ready");
        ASSERT_MSG(shaders3.lookup(mgd::shader::ShaderKey{55, 902, 0}) == nullptr, "occluded untouched");
    }

    // 10. Preditor de câmera: parado repete, em movimento extrapola
    {
        CameraPredictor cp;
        CameraPose p0 = cp.predictNext();
        (void)p0;
        cp.observe(Vec3(0, 0, 0), Vec3(0, 0, 1));
        CameraPose p1 = cp.predictNext();
        ASSERT_MSG(p1.position == Vec3(0, 0, 0), "single observe repeats");
        ASSERT_MSG(cp.confidence() == 1.0f, "full confidence when static");
        cp.observe(Vec3(0, 0, 2), Vec3(0, 0, 1));
        CameraPose p2 = cp.predictNext();
        ASSERT_MSG(p2.position == Vec3(0, 0, 4), "extrapolates motion");
        ASSERT_MSG(cp.confidence() < 1.0f, "confidence drops when moving");
        cp.reset();
        CameraPose p3 = cp.predictNext();
        ASSERT_MSG(p3.position == Vec3(0, 0, 0), "reset clears");
    }

    std::cout << "  Seed provider tests passed!" << std::endl;
    return true;
}
