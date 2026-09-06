#include <iostream>
#include <chrono>
#include <cstdio>
#include <vector>
#include <random>
#include "../core/query/Polygon.h"
#include "../core/query/RegionPolygonCache.h"
#include "../core/query/PolygonConsultant.h"
#include "../core/query/dna/Dna.h"
#include "../core/query/dna/XyzIndex.h"
#include "../core/query/dna/ColorCode.h"
#include "../core/query/dna/PixelMap.h"
#include "../core/query/dna/PixelCache.h"
#include "../core/query/dna/ChangeDetector.h"
#include "../core/query/dna/DnaPipeline.h"
#include "../core/scanner/AssetRegistry.h"
#include <algorithm>

using namespace mgd;
using Clock = std::chrono::high_resolution_clock;

static void runBenchmark(size_t numQueries, RegionPolygonCache& cache, PolygonConsultant& consultant, const std::vector<PolygonID>& queryIds, const std::vector<RegionID>& regionIds) {
    cache.resetStats();
    auto start = Clock::now();
    volatile size_t sink = 0; // evitar otimização
    for (size_t i=0;i<numQueries;++i) {
        PolygonID pid = queryIds[i % queryIds.size()];
        auto opt = consultant.findPolygon(pid);
        if (opt) sink += opt->polygon_id;
        // também consulta por região (50% das queries)
        if (i % 2 == 0) {
            RegionID rid = regionIds[i % regionIds.size()];
            auto& polys = consultant.queryRegion(rid);
            sink += polys.size();
        } else {
            // miss proposital a cada 10%
            if (i % 10 == 0) {
                auto miss = consultant.findPolygon(999999);
                (void)miss;
            }
        }
    }
    auto end = Clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    double avgUs = (totalMs * 1000.0) / numQueries;
    double throughput = numQueries / (totalMs / 1000.0);
    std::cout << "  Queries: " << numQueries << "\n";
    std::cout << "    total: " << totalMs << " ms\n";
    std::cout << "    avg: " << avgUs << " us\n";
    std::cout << "    throughput: " << throughput << " q/s\n";
    std::cout << "    hits: " << cache.hits() << " misses: " << cache.misses() << " sink=" << sink << "\n";
}

int main() {
    std::cout << "=== PolygonConsultant Benchmark ===\n";
    // Setup: 10 regiões, 1000 polígonos cada = 10k polígonos
    RegionPolygonCache cache;
    AssetRegistry registry;
    // fake assets
    for (int i=0;i<10;i++) {
        FileInfo f; f.path="a"+std::to_string(i)+".nif"; f.filename=f.path; f.extension=".nif";
        RawAnalysisResult r; r.file=f; r.detected_type=AssetType::MESH; r.success=true;
        registry.registerAsset(f, r);
    }
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> posDist(-5000, 5000);
    std::vector<RegionID> regionIds;
    std::vector<PolygonID> allIds;
    PolygonID nextPid = 1;
    for (int r=0;r<10;r++) {
        RegionID rid = 1000 + r;
        regionIds.push_back(rid);
        for (int j=0;j<1000;j++) {
            Polygon p;
            p.position = Vec3((float)posDist(rng), 0, (float)posDist(rng));
            p.polygon_id = nextPid++;
            p.asset_id = 1 + (r % 10);
            p.flags = (j%2==0)? PolygonFlag::VISIBLE : PolygonFlag::STATIC;
            cache.insert(rid, p);
            allIds.push_back(p.polygon_id);
        }
    }
    std::cout << "Setup: " << cache.regionCount() << " regions, " << cache.polygonCount() << " polygons\n";
    std::cout << "Baseline ~30 ms (headless). Medindo consultador:\n";

    PolygonConsultant consultant(&cache, &registry);
    std::vector<size_t> sizes = {1000, 10000, 100000, 1000000};
    for (size_t n : sizes) {
        runBenchmark(n, cache, consultant, allIds, regionIds);
    }

    // Teste posição -> região -> polígonos (sem varrer global)
    {
        cache.resetStats();
        auto start = Clock::now();
        volatile size_t s=0;
        for (int i=0;i<100000;i++) {
            Vec3 pos((float)posDist(rng),0,(float)posDist(rng));
            auto& polys = consultant.queryByPosition(pos);
            s += polys.size();
        }
        auto end = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(end-start).count();
        std::cout << "  queryByPosition 100k: " << ms << " ms hits=" << cache.hits() << " misses=" << cache.misses() << " sink=" << s << "\n";
    }

    // ===== Etapas incrementais: DNA + índices + mapa de pixels + cores + cache + painter =====
    // Test scene: 2000 polígonos estáticos + 50 móveis, framebuffer 96x54.
    std::cout << "=== DNA pipeline stages ===\n";
    {
        using namespace dna;
        const int FBW = 96, FBH = 54;
        std::vector<Polygon> scene;
        scene.reserve(2050);
        for (uint32_t i = 1; i <= 2000; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>((i * 37) % 100), 0.0f, static_cast<float>((i * 53) % 100));
            p.polygon_id = 5000 + i;
            p.asset_id = 1 + (i % 10);
            p.flags = PolygonFlag::VISIBLE | PolygonFlag::STATIC;
            scene.push_back(p);
        }
        for (uint32_t i = 1; i <= 50; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
            p.polygon_id = 9000 + i;
            p.asset_id = 2;
            p.flags = PolygonFlag::VISIBLE;
            scene.push_back(p);
        }

        auto t0 = Clock::now();
        XyzIndex xyz(0.25f);
        std::vector<DnaPolygon> dnas;
        dnas.reserve(scene.size());
        for (auto& p : scene) {
            DnaPolygon d;
            d.polygon_id = p.polygon_id;
            d.asset_id = p.asset_id;
            d.xyz_id = xyz.intern(p.position);
            d.geo_code = static_cast<uint16_t>(p.polygon_id % 8u);
            d.color_code = ColorCode::encode(10, static_cast<int>(p.polygon_id % 5) - 2);
            d.flags = p.flags;
            dnas.push_back(d);
        }
        auto t1 = Clock::now();
        PixelMap pmap;
        pmap.buildStatic(dnas, xyz, FBW, FBH, 4, 4);
        auto t2 = Clock::now();
        // Sistema de cores: decode de 10k códigos via LUT
        volatile uint32_t colorSink = 0;
        for (int i = 0; i < 10000; ++i) {
            RGBA c = ColorCode::decode(ColorCode::encode(10, (i % 7) - 3));
            colorSink += c.r + c.g + c.b;
        }
        auto t3 = Clock::now();
        auto ms = [](auto a, auto b){ return std::chrono::duration<double,std::milli>(b-a).count(); };
        std::cout << "  DNA + indices (" << dnas.size() << " polys, " << xyz.size() << " xyz únicos): "
                  << ms(t0,t1) << " ms\n";
        std::cout << "  Mapa de pixels (" << pmap.polygonCount() << " polys): " << ms(t1,t2) << " ms\n";
        std::cout << "  Sistema de cores (10k LUT): " << ms(t2,t3) << " ms sink=" << colorSink << "\n";

        // Cache incremental: frame cheio vs frame estático
        IncrementalPixelCache pcache(FBW, FBH);
        for (auto& d : dnas) for (auto& s : pmap.get(d.polygon_id)) pcache.setPixelCode(s.pixel_index, s.color_code);
        Framebuffer fb(FBW, FBH);
        fb.clear(RGBA(0,0,0,255));
        auto t4 = Clock::now();
        uint32_t wFull = pcache.flush(fb);
        auto t5 = Clock::now();
        for (auto& d : dnas) for (auto& s : pmap.get(d.polygon_id)) pcache.setPixelCode(s.pixel_index, s.color_code);
        uint32_t wStatic = pcache.flush(fb);
        auto t6 = Clock::now();
        std::cout << "  Cache incremental cheio: " << ms(t4,t5) << " ms written=" << wFull << "\n";
        std::cout << "  Cache incremental estático: " << ms(t5,t6) << " ms written=" << wStatic << "\n";

        // Painter (test scene): 120 frames, 50 móveis, resto estático — FPS médio + 1% low
        DnaPipeline pipe(FBW, FBH);
        pipe.buildFromPolygons(scene, 10);
        const int FRAMES = 120;
        std::vector<double> frameMs;
        frameMs.reserve(FRAMES);
        std::vector<PolygonID> moving;
        for (uint32_t i = 1; i <= 50; ++i) moving.push_back(9000 + i);
        // warmup: primeiro frame calcula tudo
        pipe.renderFrame({});
        for (int f = 0; f < FRAMES; ++f) {
            auto fs = Clock::now();
            // só os 50 móveis mudam por frame (mundo/objeto parados)
            pipe.renderFrame(moving);
            auto fe = Clock::now();
            frameMs.push_back(ms(fs, fe));
        }
        std::sort(frameMs.begin(), frameMs.end());
        double sum = 0; for (double v : frameMs) sum += v;
        double avg = sum / FRAMES;
        size_t low1 = static_cast<size_t>(FRAMES * 0.01);
        if (low1 < 1) low1 = 1;
        double worst = 0; for (size_t i = FRAMES - low1; i < frameMs.size(); ++i) worst += frameMs[i];
        worst /= low1;
        double fps = 1000.0 / avg;
        double fpsLow = 1000.0 / worst;
        std::cout << "  Test scene (" << scene.size() << " polys, 50 móveis, " << FBW << "x" << FBH << ", " << FRAMES << " frames):\n";
        std::cout << "    frame médio: " << avg << " ms | FPS médio: " << fps << "\n";
        std::cout << "    1% low: " << worst << " ms | FPS 1% low: " << fpsLow << "\n";
    }

    // ===== TESTE 0..8: baseline tradicional vs cada camada ligada =====
    // TESTE 0 = MGD atual (recalcula e reescreve tudo por frame, sem DNA/cache).
    // TESTES 1..8 ligam uma camada por vez sobre a mesma cena.
    std::cout << "=== TESTE 0..8 (mesma cena 2050 polys, 96x54, 120 frames) ===\n";
    {
        using namespace dna;
        const int FBW = 96, FBH = 54, FRAMES = 120;
        std::vector<Polygon> scene;
        for (uint32_t i = 1; i <= 2000; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>((i * 37) % 100), 0.0f, static_cast<float>((i * 53) % 100));
            p.polygon_id = 5000 + i; p.asset_id = 1 + (i % 10); p.flags = PolygonFlag::VISIBLE;
            scene.push_back(p);
        }
        for (uint32_t i = 1; i <= 50; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
            p.polygon_id = 9000 + i; p.asset_id = 2; p.flags = PolygonFlag::VISIBLE;
            scene.push_back(p);
        }
        auto ms = [](auto a, auto b){ return std::chrono::duration<double,std::milli>(b-a).count(); };

        // TESTE 0: tradicional — todo frame recalcula cor por pixel e reescreve tudo
        double t0;
        {
            Framebuffer fb(FBW, FBH);
            auto s = Clock::now();
            for (int f = 0; f < FRAMES; ++f) {
                fb.clear(RGBA(0,0,0,255));
                for (auto& p : scene) {
                    int cx = static_cast<int>(p.position.x * 0.5f + 50.0f) % FBW;
                    if (cx < 0) cx += FBW;
                    int cy = static_cast<int>(p.position.z * 0.5f + 50.0f) % FBH;
                    if (cy < 0) cy += FBH;
                    RGBA c(static_cast<uint8_t>((p.polygon_id * 67u) % 256u),
                           static_cast<uint8_t>((p.polygon_id * 131u) % 256u),
                           static_cast<uint8_t>((p.polygon_id * 197u) % 256u), 255);
                    for (int oy = 0; oy < 4; ++oy) for (int ox = 0; ox < 4; ++ox)
                        fb.setPixel((cx+ox)%FBW, (cy+oy)%FBH, c);
                }
            }
            t0 = ms(s, Clock::now()) / FRAMES;
        }
        // TESTES 1+2: DNA + índices (custo único de construção)
        XyzIndex xyz(0.25f);
        std::vector<DnaPolygon> dnas; dnas.reserve(scene.size());
        auto s1 = Clock::now();
        for (auto& p : scene) {
            DnaPolygon d;
            d.polygon_id = p.polygon_id; d.asset_id = p.asset_id;
            d.xyz_id = xyz.intern(p.position);
            d.geo_code = static_cast<uint16_t>(p.polygon_id % 8u);
            d.color_code = ColorCode::encode(10, static_cast<int>(p.polygon_id % 5) - 2);
            d.flags = p.flags;
            dnas.push_back(d);
        }
        double t12 = ms(s1, Clock::now());
        // TESTE 3: mapa de pixels (custo único)
        PixelMap pmap;
        auto s3 = Clock::now();
        pmap.buildStatic(dnas, xyz, FBW, FBH, 4, 4);
        double t3 = ms(s3, Clock::now());
        // TESTE 4: cores via LUT 10k
        auto s4 = Clock::now();
        volatile uint32_t csink = 0;
        for (int i = 0; i < 10000; ++i) { RGBA c = ColorCode::decode(ColorCode::encode(10, (i%7)-3)); csink += c.r; }
        double t4 = ms(s4, Clock::now());
        (void)csink;
        // TESTE 5: pixel cache cheio
        IncrementalPixelCache pc(FBW, FBH);
        Framebuffer fb2(FBW, FBH); fb2.clear(RGBA(0,0,0,255));
        for (auto& d : dnas) for (auto& sp : pmap.get(d.polygon_id)) pc.setPixelCode(sp.pixel_index, sp.color_code);
        auto s5 = Clock::now();
        pc.flush(fb2);
        double t5 = ms(s5, Clock::now());
        // TESTE 6: framebuffer incremental estático
        for (auto& d : dnas) for (auto& sp : pmap.get(d.polygon_id)) pc.setPixelCode(sp.pixel_index, sp.color_code);
        auto s6 = Clock::now();
        uint32_t w6 = pc.flush(fb2);
        double t6 = ms(s6, Clock::now());
        // TESTES 7+8: detecção de mudanças + pipeline completo (50 móveis, 120 frames)
        DnaPipeline pipe(FBW, FBH);
        pipe.buildFromPolygons(scene, 10);
        pipe.renderFrame({});
        std::vector<PolygonID> moving;
        for (uint32_t i = 1; i <= 50; ++i) moving.push_back(9000 + i);
        std::vector<double> fms; fms.reserve(FRAMES);
        for (int f = 0; f < FRAMES; ++f) {
            auto fs = Clock::now();
            pipe.renderFrame(moving);
            fms.push_back(ms(fs, Clock::now()));
        }
        std::sort(fms.begin(), fms.end());
        double sum = 0; for (double v : fms) sum += v;
        double t78 = sum / FRAMES;
        double low = fms.back();

        std::cout << "  Versão              | ms/frame | FPS médio | 1% low\n";
        auto row = [](const char* n, double m){ printf("  %-19s | %8.4f | %9.1f | %8.4f\n", n, m, 1000.0/m, m); };
        // 1% low só medido no pipeline completo; demais usam o próprio valor
        row("TESTE 0 atual", t0);
        printf("  TESTE 1+2 DNA+XYZ    | (construção única: %.4f ms)\n", t12);
        printf("  TESTE 3 pixel map    | (construção única: %.4f ms)\n", t3);
        printf("  TESTE 4 cores LUT    | (10k: %.4f ms)\n", t4);
        printf("  TESTE 5 pixel cache  | (flush cheio: %.4f ms)\n", t5);
        printf("  TESTE 6 incremental  | %8.4f | %9.1f | (reescreve só %u)\n", t6, 1000.0/t6, w6);
        row("TESTE 7+8 completo", t78);
        printf("  1%% low pipeline     | %8.4f | %9.1f\n", low, 1000.0/low);
    }

    // ===== CENA FINAL 720p + 1080p: tudo ligado (DNA+LOD+incremental) =====
    // Mesma cena, 60 frames, 50 móveis. É o número honesto.
    for (int res = 0; res < 2; ++res) {
        const int FBW = res == 0 ? 1280 : 1920;
        const int FBH = res == 0 ? 720 : 1080;
        const int FRAMES = 60;
        std::cout << "=== CENA FINAL " << FBW << "x" << FBH << " (tudo ligado) ===\n";
        {
            using namespace dna;
        std::vector<Polygon> scene;
        for (uint32_t i = 1; i <= 2000; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>((i * 37) % 1000), 0.0f, static_cast<float>((i * 53) % 1000));
            p.polygon_id = 5000 + i; p.asset_id = 1 + (i % 10); p.flags = PolygonFlag::VISIBLE;
            scene.push_back(p);
        }
        for (uint32_t i = 1; i <= 50; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>(i * 10), 0.0f, 0.0f);
            p.polygon_id = 9000 + i; p.asset_id = 2; p.flags = PolygonFlag::VISIBLE;
            scene.push_back(p);
        }
        auto ms = [](auto a, auto b){ return std::chrono::duration<double,std::milli>(b-a).count(); };
        // Tradicional em 720p (para comparar)
        double tTrad;
        {
            Framebuffer fb(FBW, FBH);
            auto s = Clock::now();
            for (int f = 0; f < FRAMES; ++f) {
                fb.clear(RGBA(0,0,0,255));
                for (auto& p : scene) {
                    int cx = static_cast<int>(p.position.x * 0.5f + 50.0f) % FBW;
                    if (cx < 0) cx += FBW;
                    int cy = static_cast<int>(p.position.z * 0.5f + 50.0f) % FBH;
                    if (cy < 0) cy += FBH;
                    RGBA c(static_cast<uint8_t>((p.polygon_id * 67u) % 256u),
                           static_cast<uint8_t>((p.polygon_id * 131u) % 256u),
                           static_cast<uint8_t>((p.polygon_id * 197u) % 256u), 255);
                    for (int oy = 0; oy < 4; ++oy) for (int ox = 0; ox < 4; ++ox)
                        fb.setPixel((cx+ox)%FBW, (cy+oy)%FBH, c);
                }
            }
            tTrad = ms(s, Clock::now()) / FRAMES;
        }
        // Pipeline completo em 720p
        DnaPipeline pipe(FBW, FBH);
        pipe.buildFromPolygons(scene, 10);
        pipe.renderFrame({});
        std::vector<PolygonID> moving;
        for (uint32_t i = 1; i <= 50; ++i) moving.push_back(9000 + i);
        std::vector<double> fms; fms.reserve(FRAMES);
        for (int f = 0; f < FRAMES; ++f) {
            auto fs = Clock::now();
            pipe.renderFrame(moving);
            fms.push_back(ms(fs, Clock::now()));
        }
        std::sort(fms.begin(), fms.end());
        double sum = 0; for (double v : fms) sum += v;
        double avg = sum / FRAMES;
        double low = fms.back();
        std::cout << "  tradicional " << FBW << "x" << FBH << ": " << tTrad << " ms (" << 1000.0/tTrad << " FPS)\n";
        std::cout << "  pipeline " << FBW << "x" << FBH << ":    " << avg << " ms (" << 1000.0/avg << " FPS), 1% low " << low << " ms (" << 1000.0/low << " FPS)\n";
        }
    }

    return 0;
}
