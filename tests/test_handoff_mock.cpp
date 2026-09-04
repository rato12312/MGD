#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "core/bridge/EmulatorHandoff.h"
#include "core/query/RegionPolygonCache.h"
#include "core/query/PolygonConsultant.h"
#include "core/query/dna/DnaPipeline.h"

using namespace mgd;
using namespace mgd::bridge;
using namespace mgd::dna;

// Passo a passo do caminho mais fácil (sem jogo, sem celular):
// 1. Monta HandoffFrame mockado como se o Eden tivesse entregue.
// 2. Insere os polígonos visíveis no cache por região.
// 3. Consultador resolve PolygonID -> AssetID.
// 4. Pipeline DNA pinta e o segundo frame estático reutiliza tudo.
bool run_handoff_mock_tests() {
    // 1. Handoff mockado: câmera + 2 regiões + 10 polígonos visíveis
    HandoffFrame frame;
    frame.frame_index = 1;
    frame.camera.position = Vec3(0.0f, 5.0f, 15.0f);
    frame.camera.forward = Vec3(0.0f, 0.0f, 1.0f);
    frame.ui_visible = false;
    RegionID r1 = 101, r2 = 102;
    frame.visible_regions = {r1, r2};
    for (uint32_t i = 1; i <= 10; ++i) frame.visible_polygons.push_back(2000 + i);
    ASSERT_MSG(frame.visible_polygons.size() == 10, "handoff carrega 10 poligonos");

    // 2. Cache por região a partir do handoff
    RegionPolygonCache cache;
    AssetRegistry* noRegistry = nullptr;
    PolygonConsultant consultant(&cache, noRegistry);
    for (uint32_t i = 1; i <= 10; ++i) {
        Polygon p;
        p.position = Vec3(static_cast<float>(i), 0.0f, static_cast<float>(i));
        p.polygon_id = 2000 + i;
        p.asset_id = 70 + (i % 3);
        p.flags = PolygonFlag::VISIBLE;
        RegionID r = (i <= 5) ? r1 : r2;
        ASSERT_MSG(consultant.insertPolygon(p.position, r, p.polygon_id, p.asset_id, p.flags), "insert handoff polygon");
    }
    ASSERT_MSG(consultant.queryRegion(r1).size() == 5, "regiao 1 com 5");
    ASSERT_MSG(consultant.queryRegion(r2).size() == 5, "regiao 2 com 5");

    // 3. Resolve PolygonID -> AssetID sem copiar asset
    auto aid = consultant.resolveAssetId(2003);
    ASSERT_MSG(aid.has_value() && *aid == 70 + (3 % 3), "resolve AssetID do handoff");

    // 4. Pipeline pinta o handoff; frame 2 estático reutiliza
    std::vector<Polygon> scene;
    for (uint32_t i = 1; i <= 10; ++i) {
        Polygon p;
        p.position = Vec3(static_cast<float>(i), 0.0f, static_cast<float>(i));
        p.polygon_id = 2000 + i;
        p.asset_id = 70;
        p.flags = PolygonFlag::VISIBLE;
        scene.push_back(p);
    }
    DnaPipeline pipe(64, 32);
    pipe.buildFromPolygons(scene, 10);
    DnaFrameStats s1 = pipe.renderFrame({});
    ASSERT_MSG(s1.pixels_written > 0, "handoff pinta pixels no frame 1");
    DnaFrameStats s2 = pipe.renderFrame({});
    ASSERT_MSG(s2.rebuilt_polys == 0 && s2.pixels_written == 0, "frame 2 estatico reutiliza tudo");

    std::cout << "  Handoff mock tests passed!" << std::endl;
    return true;
}
