// Demo do port: boot (serviços+FS+relógio+GPU) e um frame pintado pelo DNA.
// Uso: port_demo — prova que as peças conversam de verdade.
#include <cstdio>
#include <string>
#include <vector>
#include "../src/runtime/BootSequence.h"
#include "../src/runtime/ServiceRegistry.h"
#include "../src/runtime/VirtualFs.h"
#include "../src/runtime/SystemClock.h"
#include "../src/runtime/GpuCaps.h"
#include "../src/mgd/query/Polygon.h"
#include "../src/mgd/query/RegionPolygonCache.h"
#include "../src/mgd/query/dna/DnaPipeline.h"

namespace {
port::ServiceRegistry g_services;
port::VirtualFs g_fs;
port::SystemClock g_clock;

bool stepServices(std::string& note) {
    g_services.call("fs"); g_services.call("set"); g_services.call("time");
    g_services.call("audio"); g_services.call("vi"); g_services.call("am");
    g_services.call("hid");
    note = "7 servicos";
    return true;
}
bool stepFs(std::string& note) {
    g_fs.mount("save:/", "/data/save");
    g_fs.mount("rom:/", "/data/rom");
    if (!g_fs.resolve("rom:/Data/Stage.szs")) { note = "rom:/ falhou"; return false; }
    note = "rom:/ ok";
    return true;
}
bool stepClock(std::string& note) {
    g_clock.tick(19200000ull);
    note = "1s";
    return true;
}
bool stepGpu(std::string& note) {
    port::GpuCaps caps;
    if (caps.fallbackFor("multiViewport") != port::GpuFallback::SingleViewport) {
        note = "sem fallback";
        return false;
    }
    note = "mali-amigavel";
    return true;
}
} // namespace

int main() {
    port::BootSequence boot;
    boot.addStep(port::BootStage::Services, stepServices);
    boot.addStep(port::BootStage::Filesystem, stepFs);
    boot.addStep(port::BootStage::Clock, stepClock);
    boot.addStep(port::BootStage::Gpu, stepGpu);
    if (!boot.run()) {
        std::printf("boot FALHOU em %zu etapas\n", boot.passed());
        return 1;
    }
    std::printf("boot OK (%zu etapas)\n", boot.passed());

    // Cena: 200 polígonos, 10 móveis. Pipeline DNA pinta 3 frames.
    std::vector<mgd::Polygon> scene;
    for (uint32_t i = 1; i <= 200; ++i) {
        mgd::Polygon p;
        p.position = mgd::Vec3(static_cast<float>((i * 37) % 100), 0.0f,
                               static_cast<float>((i * 53) % 100));
        p.polygon_id = 1000 + i;
        p.asset_id = 1 + (i % 5);
        p.flags = mgd::PolygonFlag::VISIBLE;
        scene.push_back(p);
    }
    mgd::dna::DnaPipeline pipe(96, 54);
    pipe.buildFromPolygons(scene, 10);
    auto s1 = pipe.renderFrame({});
    std::printf("frame 1: rebuilt=%u written=%u\n", s1.rebuilt_polys, s1.pixels_written);
    std::vector<mgd::PolygonID> moving;
    for (uint32_t i = 0; i < 10; ++i) moving.push_back(1001 + i);
    auto s2 = pipe.renderFrame(moving);
    std::printf("frame 2: rebuilt=%u reused=%u written=%u\n",
                s2.rebuilt_polys, s2.reused_polys, s2.pixels_written);
    auto s3 = pipe.renderFrame({});
    std::printf("frame 3: rebuilt=%u reused=%u written=%u\n",
                s3.rebuilt_polys, s3.reused_polys, s3.pixels_written);
    std::printf("port demo OK\n");
    return 0;
}
