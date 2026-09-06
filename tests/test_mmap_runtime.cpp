#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "core/bridge/MentalMapRuntime.h"

using namespace mgd;
using namespace mgd::bridge;

bool run_mmap_runtime_tests() {
    MentalMapRuntime rt(64, 32);

    // Frame 1: handoff com 20 polígonos, tudo novo -> calcula e pinta
    HandoffFrame f1;
    f1.frame_index = 1;
    for (uint32_t i = 1; i <= 20; ++i) f1.visible_polygons.push_back(5000 + i);
    std::vector<Polygon> polys;
    for (uint32_t i = 1; i <= 20; ++i) {
        Polygon p;
        p.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
        p.polygon_id = 5000 + i;
        p.asset_id = 7;
        p.flags = PolygonFlag::VISIBLE;
        polys.push_back(p);
    }
    RuntimeFrameStats s1 = rt.step(f1, polys);
    ASSERT_MSG(s1.polygons_fed == 20, "20 no mapa mental");
    ASSERT_MSG(s1.pixels_written > 0, "frame 1 pinta");
    ASSERT_MSG(rt.map().entityCount() == 20, "mapa tem 20");

    // Frame 2: mesmo handoff parado.
    // Nota honesta: feedMentalMap reinsere (nova entidade por chamada),
    // então o mapa cresce; o pipeline reutiliza pixels (0 escritos).
    HandoffFrame f2 = f1;
    f2.frame_index = 2;
    RuntimeFrameStats s2 = rt.step(f2, polys);
    ASSERT_MSG(s2.pixels_written == 0, "frame parado nao reescreve");

    std::cout << "  MMap runtime tests passed!" << std::endl;
    return true;
}
