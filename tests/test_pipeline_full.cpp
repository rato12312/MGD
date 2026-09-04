#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "core/bridge/EmulatorHandoff.h"
#include "core/query/RegionPolygonCache.h"
#include "core/query/PolygonConsultant.h"
#include "core/query/dna/DnaPipeline.h"
#include "core/painter/shader/ShaderCache.h"

using namespace mgd;
using namespace mgd::bridge;
using namespace mgd::dna;
using namespace mgd::shader;

// Teste bom mesmo: escala realista (5000 estáticos + 200 móveis),
// handoff -> consultador -> DNA -> shader cache -> incremental.
bool run_pipeline_full_tests() {
    RegionPolygonCache cache;
    PolygonConsultant consultant(&cache, nullptr);
    ShaderCache shaders(8192);

    // 1. Handoff com 5200 polígonos em 4 regiões
    HandoffFrame frame;
    frame.frame_index = 1;
    std::vector<RegionID> regions = {11, 12, 13, 14};
    frame.visible_regions = regions;
    uint32_t pid = 1;
    for (size_t r = 0; r < regions.size(); ++r) {
        for (int i = 0; i < 1300; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>((pid * 37) % 200), 0.0f,
                              static_cast<float>((pid * 53) % 200));
            p.polygon_id = 10000 + pid;
            p.asset_id = 1 + (pid % 20);
            p.flags = PolygonFlag::VISIBLE | PolygonFlag::STATIC;
            ASSERT_MSG(consultant.insertPolygon(p.position, regions[r], p.polygon_id, p.asset_id, p.flags), "insert escala");
            frame.visible_polygons.push_back(p.polygon_id);
            ++pid;
        }
    }
    ASSERT_MSG(frame.visible_polygons.size() == 5200, "handoff com 5200");
    ASSERT_MSG(cache.polygonCount() == 5200, "cache com 5200");

    // 2. Shader cache: primeiro miss, depois hit para o mesmo asset/polígono
    ShaderKey sk{5, 10001, 0};
    ASSERT_MSG(shaders.lookup(sk) == nullptr, "shader miss inicial");
    shaders.store(sk, 4242, 0xFF);
    ASSERT_MSG(shaders.lookup(sk) != nullptr, "shader hit após store");
    ASSERT_MSG(shaders.lookup(sk)->pipeline_code == 4242, "pipeline preservado");

    // 3. Pipeline completo em cima da cena do handoff
    std::vector<Polygon> scene;
    for (RegionID r : regions) {
        const auto& polys = consultant.queryRegion(r);
        for (const auto& p : polys) scene.push_back(p);
    }
    ASSERT_MSG(scene.size() == 5200, "cena completa do consultador");
    DnaPipeline pipe(160, 90);
    pipe.buildFromPolygons(scene, 10);
    DnaFrameStats s1 = pipe.renderFrame({});
    ASSERT_MSG(s1.rebuilt_polys == 5200, "frame 1 calcula tudo");
    ASSERT_MSG(s1.pixels_written > 0, "frame 1 escreve");
    DnaFrameStats s2 = pipe.renderFrame({});
    ASSERT_MSG(s2.rebuilt_polys == 0 && s2.pixels_written == 0, "frame 2 estático zera");

    // 4. 200 móveis: só eles recalculam
    std::vector<PolygonID> moving;
    for (uint32_t i = 0; i < 200; ++i) moving.push_back(10001 + i);
    DnaFrameStats s3 = pipe.renderFrame(moving);
    ASSERT_MSG(s3.rebuilt_polys == 200, "só móveis recalculam");
    ASSERT_MSG(s3.reused_polys == 5000, "estáticos reutilizados");

    std::cout << "  Pipeline full tests passed!" << std::endl;
    return true;
}
