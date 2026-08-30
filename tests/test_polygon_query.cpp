#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/query/Polygon.h"
#include "core/query/RegionPolygonCache.h"
#include "core/query/PolygonConsultant.h"
#include "core/mental_map/MentalMap.h"
#include "core/mental_map/ChunkManager.h"
#include "core/scanner/AssetRegistry.h"

using namespace mgd;

bool run_polygon_query_tests() {
    RegionPolygonCache cache;
    AssetRegistry registry;
    PolygonConsultant consultant(&cache, &registry);

    // Setup fake assets for AssetID resolution (1..3)
    FileInfo f1; f1.path="a.nif"; f1.filename="a.nif"; f1.extension=".nif";
    RawAnalysisResult r1; r1.file=f1; r1.detected_type=AssetType::MESH; r1.success=true; r1.spatial_bounds=AABB(Vec3(0,0,0), Vec3(1,1,1));
    uint32_t aid1 = registry.registerAsset(f1, r1);
    FileInfo f2; f2.path="b.nif"; f2.filename="b.nif"; f2.extension=".nif";
    RawAnalysisResult r2; r2.file=f2; r2.detected_type=AssetType::MESH; r2.success=true; r2.spatial_bounds=AABB(Vec3(5,0,0), Vec3(6,0,0));
    uint32_t aid2 = registry.registerAsset(f2, r2);
    FileInfo f3; f3.path="c.nif"; f3.filename="c.nif"; f3.extension=".nif";
    RawAnalysisResult r3; r3.file=f3; r3.detected_type=AssetType::MESH; r3.success=true; r3.spatial_bounds=AABB(Vec3(10,0,0), Vec3(11,0,0));
    uint32_t aid3 = registry.registerAsset(f3, r3);

    RegionID regionA = 100;
    RegionID regionB = 200;

    // 1. Inserir polígonos em uma região
    Polygon p1; p1.position=Vec3(0,0,0); p1.polygon_id=101; p1.asset_id=aid1; p1.flags=PolygonFlag::VISIBLE;
    Polygon p2; p2.position=Vec3(1,0,0); p2.polygon_id=102; p2.asset_id=aid1; p2.flags=PolygonFlag::STATIC;
    Polygon p3; p3.position=Vec3(5,0,0); p3.polygon_id=103; p3.asset_id=aid2; p3.flags=0;
    ASSERT_MSG(cache.insert(regionA, p1), "insert p1");
    ASSERT_MSG(cache.insert(regionA, p2), "insert p2");
    ASSERT_MSG(cache.insert(regionA, p3), "insert p3");
    ASSERT_EQ(cache.polygonCount(regionA), static_cast<size_t>(3));
    ASSERT_EQ(cache.regionCount(), static_cast<size_t>(1));

    // 2. Consultar uma região (sem cópia, sem alocação extra)
    {
        const auto& polys = consultant.queryRegion(regionA);
        ASSERT_EQ(polys.size(), static_cast<size_t>(3));
        // cache hit
        ASSERT_MSG(cache.hits() > 0, "cache hit for region");
    }

    // 3. Encontrar PolygonID
    {
        auto opt = consultant.findPolygon(101);
        ASSERT_MSG(opt.has_value(), "find PolygonID 101");
        ASSERT_EQ(opt->polygon_id, static_cast<PolygonID>(101));
        ASSERT_MSG(consultant.findPolygonPtr(999) == nullptr, "not found 999");
    }

    // 4. Resolver PolygonID -> AssetID
    {
        auto aid = consultant.resolveAssetId(101);
        ASSERT_MSG(aid.has_value(), "resolve 101 -> AssetID");
        ASSERT_EQ(*aid, aid1);
        auto aidMissing = consultant.resolveAssetId(999);
        ASSERT_MSG(!aidMissing.has_value(), "missing polygon -> no AssetID");
    }

    // 5. Resolver AssetID -> asset
    {
        auto asset = consultant.resolveAsset(101);
        ASSERT_MSG(asset.has_value(), "resolve 101 -> asset");
        ASSERT_EQ(asset->file.path, std::string("a.nif"));
        auto asset2 = consultant.resolveAssetById(aid2);
        ASSERT_MSG(asset2.has_value(), "resolve by AssetID");
        ASSERT_EQ(asset2->file.path, std::string("b.nif"));
        auto miss = consultant.resolveAssetById(9999);
        ASSERT_MSG(!miss.has_value(), "missing asset");
    }

    // 6. Diferenciar polígonos que possuem posições iguais (mesma posição, IDs diferentes)
    {
        Polygon dupPos1; dupPos1.position=Vec3(0,0,0); dupPos1.polygon_id=201; dupPos1.asset_id=aid3; dupPos1.flags=PolygonFlag::VISIBLE;
        Polygon dupPos2; dupPos2.position=Vec3(0,0,0); dupPos2.polygon_id=202; dupPos2.asset_id=aid3; dupPos2.flags=PolygonFlag::OCCLUDED;
        ASSERT_MSG(cache.insert(regionA, dupPos1), "insert dup pos 201");
        ASSERT_MSG(cache.insert(regionA, dupPos2), "insert dup pos 202");
        ASSERT_EQ(cache.polygonCount(regionA), static_cast<size_t>(5));
        auto o1 = consultant.findPolygon(201);
        auto o2 = consultant.findPolygon(202);
        ASSERT_MSG(o1.has_value() && o2.has_value(), "both dup pos found");
        ASSERT_MSG(o1->position == o2->position, "same position");
        ASSERT_MSG(o1->polygon_id != o2->polygon_id, "different PolygonID");
        ASSERT_MSG(o1->flags != o2->flags, "different flags");
    }

    // 7. Consultar regiões diferentes (isolamento)
    {
        Polygon pb1; pb1.position=Vec3(100,0,0); pb1.polygon_id=301; pb1.asset_id=aid2; pb1.flags=0;
        ASSERT_MSG(cache.insert(regionB, pb1), "insert regionB");
        auto polysA = consultant.queryRegion(regionA);
        auto polysB = consultant.queryRegion(regionB);
        ASSERT_EQ(polysA.size(), static_cast<size_t>(5));
        ASSERT_EQ(polysB.size(), static_cast<size_t>(1));
        ASSERT_MSG(polysB[0].polygon_id == 301, "regionB has 301");
        // posição -> região via ChunkManager
        Vec3 posA(0,0,0);
        RegionID rA = ChunkManager::worldToRegionId(posA);
        // rA may not be 100/200, but queryByPosition should use same mapping
        // Insert via position API
        Polygon pPos; pPos.position=Vec3(5000,0,5000); pPos.polygon_id=401; pPos.asset_id=aid1;
        RegionID rPos = ChunkManager::worldToRegionId(pPos.position);
        ASSERT_MSG(consultant.insertPolygon(pPos.position, rPos, 401, aid1, 0), "insert via position");
        auto q = consultant.queryByPosition(pPos.position);
        bool found=false; for(auto &pp:q) if(pp.polygon_id==401) found=true;
        ASSERT_MSG(found, "queryByPosition found 401");
    }

    // 8. Verificar cache hit (consultar região existente)
    {
        cache.resetStats();
        auto &polys = consultant.queryRegion(regionA);
        (void)polys;
        ASSERT_MSG(cache.hits() >= 1, "hit increment");
        ASSERT_EQ(cache.misses(), static_cast<uint64_t>(0));
    }

    // 9. Verificar cache miss (região inexistente e PolygonID inexistente)
    {
        cache.resetStats();
        auto &polys = consultant.queryRegion(9999);
        ASSERT_EQ(polys.size(), static_cast<size_t>(0));
        ASSERT_MSG(cache.misses() >= 1, "miss for unknown region");
        cache.resetStats();
        auto opt = consultant.findPolygon(99999);
        ASSERT_MSG(!opt.has_value(), "miss polygon");
        ASSERT_MSG(cache.misses() >= 1, "miss for unknown PolygonID");
    }

    // 10. Verificar flags/estado (BitSpace)
    {
        Polygon pf; pf.position=Vec3(9,0,0); pf.polygon_id=501; pf.asset_id=aid1; pf.flags=0;
        pf.setFlag(PolygonFlag::VISIBLE);
        pf.setFlag(PolygonFlag::COLLIDABLE);
        ASSERT_MSG(pf.hasFlag(PolygonFlag::VISIBLE), "has VISIBLE");
        ASSERT_MSG(pf.hasFlag(PolygonFlag::COLLIDABLE), "has COLLIDABLE");
        ASSERT_MSG(!pf.hasFlag(PolygonFlag::OCCLUDED), "not OCCLUDED");
        pf.setFlag(PolygonFlag::OCCLUDED);
        ASSERT_MSG(pf.hasFlag(PolygonFlag::OCCLUDED), "now OCCLUDED");
        pf.clearFlag(PolygonFlag::VISIBLE);
        ASSERT_MSG(!pf.hasFlag(PolygonFlag::VISIBLE), "cleared VISIBLE");
        // distância preparada para LOD futuro
        float d = pf.distanceTo(Vec3(0,0,0));
        ASSERT_NEAR(d, 9.0f, 0.01f);
        // inserir e verificar persistência de flags via cache
        RegionID r = 300;
        ASSERT_MSG(cache.insert(r, pf), "insert flagged");
        auto found = consultant.findPolygon(501);
        ASSERT_MSG(found.has_value(), "found flagged");
        ASSERT_MSG(found->hasFlag(PolygonFlag::OCCLUDED), "flag persisted");
        ASSERT_MSG(found->hasFlag(PolygonFlag::COLLIDABLE), "flag2 persisted");
        ASSERT_MSG(!found->hasFlag(PolygonFlag::VISIBLE), "flag cleared persisted");
    }

    // Integração com Mapa Mental sem duplicar assets (referência)
    {
        MentalMap map;
        // feed single polygon
        RegionID r = 400;
        Polygon pm; pm.position=Vec3(2,0,0); pm.polygon_id=601; pm.asset_id=aid1; pm.flags=PolygonFlag::STATIC;
        cache.insert(r, pm);
        EntityID eid = consultant.feedMentalMap(map, 601);
        ASSERT_MSG(eid != INVALID_ENTITY_ID, "feedMentalMap single");
        ASSERT_EQ(map.entityCount(), static_cast<size_t>(1));
        auto ent = map.getEntity(eid);
        ASSERT_MSG(ent.has_value(), "entity exists");
        ASSERT_MSG(ent->get().resource_id == aid1, "resource_id == AssetID (sem cópia)");
        ASSERT_MSG(ent->get().transform.position == pm.position, "position preserved");
        // feed região inteira
        MentalMap map2;
        size_t n = consultant.feedMentalMapRegion(map2, r);
        ASSERT_EQ(n, static_cast<size_t>(1));
        ASSERT_EQ(map2.entityCount(), static_cast<size_t>(1));
    }

    std::cout << "  Polygon query tests passed!" << std::endl;
    return true;
}
