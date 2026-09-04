#include <iostream>
#include <string>
#include <cmath>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "core/query/dna/Dna.h"
#include "core/query/dna/XyzIndex.h"
#include "core/query/dna/ColorCode.h"
#include "core/query/dna/PixelMap.h"
#include "core/query/dna/PixelCache.h"
#include "core/query/dna/ChangeDetector.h"
#include "core/query/dna/DnaPipeline.h"
#include "core/query/Polygon.h"

using namespace mgd;
using namespace mgd::dna;

bool run_dna_pipeline_tests() {
    // 1. DNA + índices X/Y/Z: mesma posição -> mesmo índice; decode volta perto
    {
        XyzIndex xyz(0.25f);
        uint32_t a = xyz.intern(Vec3(1.0f, 2.0f, 3.0f));
        uint32_t b = xyz.intern(Vec3(1.01f, 2.01f, 3.01f)); // mesmo voxel
        ASSERT_MSG(a == b, "same voxel -> same xyz_id");
        Vec3 d = xyz.decode(a);
        ASSERT_MSG(std::abs(d.x - 1.0f) < 0.26f, "decode x near");
        ASSERT_MSG(xyz.size() == 1, "dedup saves memory");
    }

    // 2. Cor por código: base + variação determinística via LUT
    {
        uint16_t c0 = ColorCode::encode(10, 0);
        uint16_t c1 = ColorCode::encode(10, 1);
        uint16_t c2 = ColorCode::encode(10, -1);
        ASSERT_MSG(c0 != c1 && c0 != c2, "variation changes code");
        RGBA r0 = ColorCode::decode(c0);
        RGBA r1 = ColorCode::decode(c0);
        ASSERT_MSG(r0 == r1, "LUT deterministic, no recompute");
        ASSERT_MSG(ColorCode::baseOf(c0) == 10, "base preserved");
    }

    // 3. Mapa de pixels pré-calculado para estáticos
    {
        XyzIndex xyz;
        std::vector<DnaPolygon> dnas;
        DnaPolygon d; d.polygon_id = 7; d.asset_id = 3;
        d.xyz_id = xyz.intern(Vec3(0, 0, 0));
        d.color_code = ColorCode::encode(10, 0);
        dnas.push_back(d);
        PixelMap pm;
        pm.buildStatic(dnas, xyz, 64, 64, 4, 4);
        ASSERT_MSG(pm.polygonCount() == 1, "pixelmap has polygon");
        ASSERT_MSG(pm.get(7).size() == 16, "4x4 block precomputed");
        ASSERT_MSG(pm.get(999).empty(), "unknown polygon empty");
    }

    // 4. Cache incremental: só pixels alterados são escritos
    {
        IncrementalPixelCache cache(8, 8);
        for (uint32_t i = 0; i < 10; ++i) cache.setPixelCode(i, ColorCode::encode(10, 0));
        ASSERT_MSG(cache.dirtyCount() == 10, "10 dirty");
        Framebuffer fb(8, 8);
        fb.clear(RGBA(0, 0, 0, 255));
        uint32_t w1 = cache.flush(fb);
        ASSERT_MSG(w1 == 10, "writes only dirty");
        ASSERT_MSG(cache.dirtyCount() == 0, "clean after flush");
        for (uint32_t i = 0; i < 10; ++i) cache.setPixelCode(i, ColorCode::encode(10, 0));
        ASSERT_MSG(cache.dirtyCount() == 0, "same code -> no rewrite");
        uint32_t w2 = cache.flush(fb);
        ASSERT_MSG(w2 == 0, "zero writes when nothing changed");
    }

    // 5. Detecção de mudanças: mundo/objeto/polígono
    {
        ChangeDetector cd;
        ASSERT_MSG(cd.polygonChanged(1, cd.worldVersion(), cd.polygonVersion(1)), "new polygon needs calc");
        cd.touchPolygon(1);
        uint64_t pv = cd.polygonVersion(1);
        ASSERT_MSG(!cd.polygonChanged(1, cd.worldVersion(), pv), "reuse when unchanged");
        cd.touchWorld();
        ASSERT_MSG(cd.polygonChanged(1, cd.worldVersion(), pv), "world change forces recalc");
    }

    // 6. Pipeline completo: frame 1 calcula tudo, frame 2 estático reutiliza
    {
        std::vector<Polygon> polys;
        for (uint32_t i = 1; i <= 20; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
            p.polygon_id = 1000 + i;
            p.asset_id = 50;
            p.flags = PolygonFlag::VISIBLE;
            polys.push_back(p);
        }
        DnaPipeline pipe(64, 64);
        pipe.buildFromPolygons(polys, 10);
        DnaFrameStats s1 = pipe.renderFrame({}); // tudo novo -> calcula
        ASSERT_MSG(s1.rebuilt_polys == 20, "first frame builds all");
        ASSERT_MSG(s1.pixels_written > 0, "first frame writes");
        DnaFrameStats s2 = pipe.renderFrame({}); // nada mudou -> reutiliza
        ASSERT_MSG(s2.rebuilt_polys == 0, "static frame reuses DNA");
        ASSERT_MSG(s2.reused_polys == 20, "all reused");
        ASSERT_MSG(s2.pixels_written == 0, "no rewrite when static");
        // muda 1 polígono -> só ele recalcula
        DnaFrameStats s3 = pipe.renderFrame({1005});
        ASSERT_MSG(s3.rebuilt_polys == 1, "only changed polygon rebuilt");
        ASSERT_MSG(s3.pixels_written > 0, "changed pixels written");
    }

    std::cout << "  DNA pipeline tests passed!" << std::endl;
    return true;
}
