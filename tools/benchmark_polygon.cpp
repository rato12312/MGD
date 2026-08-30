#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include "../core/query/Polygon.h"
#include "../core/query/RegionPolygonCache.h"
#include "../core/query/PolygonConsultant.h"
#include "../core/scanner/AssetRegistry.h"

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

    return 0;
}
