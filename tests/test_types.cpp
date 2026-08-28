#include <iostream>
#include <string>
#include <cmath>
#include <cstdint>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/common/Types.h"

using namespace mgd;

bool run_types_tests() {
    EntityID eid = 5;
    ASSERT_EQ(eid, static_cast<EntityID>(5));

    ResourceID rid = 10;
    ASSERT_EQ(rid, static_cast<ResourceID>(10));

    MeshID mid = 3;
    ASSERT_EQ(mid, static_cast<MeshID>(3));

    TextureID tid = 7;
    ASSERT_EQ(tid, static_cast<TextureID>(7));

    MaterialID matid = 11;
    ASSERT_EQ(matid, static_cast<MaterialID>(11));

    CollisionID cid = 2;
    ASSERT_EQ(cid, static_cast<CollisionID>(2));

    RegionID regid = 4;
    ASSERT_EQ(regid, static_cast<RegionID>(4));

    AnimationID aid = 9;
    ASSERT_EQ(aid, static_cast<AnimationID>(9));

    ASSERT_EQ(INVALID_ENTITY_ID, static_cast<EntityID>(0));
    ASSERT_EQ(INVALID_MESH_ID, static_cast<MeshID>(0));
    ASSERT_EQ(INVALID_TEXTURE_ID, static_cast<TextureID>(0));
    ASSERT_EQ(INVALID_MATERIAL_ID, static_cast<MaterialID>(0));
    ASSERT_EQ(INVALID_COLLISION_ID, static_cast<CollisionID>(0));
    ASSERT_EQ(INVALID_REGION_ID, static_cast<RegionID>(0));

    EntityState es0 = EntityState::ACTIVE;
    EntityState es1 = EntityState::INACTIVE;
    EntityState es2 = EntityState::HIDDEN;
    EntityState es3 = EntityState::DISABLED;
    EntityState es4 = EntityState::DIRTY;
    ASSERT_EQ(static_cast<uint8_t>(es0), 0);
    ASSERT_EQ(static_cast<uint8_t>(es1), 1);
    ASSERT_EQ(static_cast<uint8_t>(es2), 2);
    ASSERT_EQ(static_cast<uint8_t>(es3), 3);
    ASSERT_EQ(static_cast<uint8_t>(es4), 4);

    VisibilityState vs0 = VisibilityState::UNCHECKED;
    VisibilityState vs1 = VisibilityState::VISIBLE;
    VisibilityState vs2 = VisibilityState::OCCLUDED;
    VisibilityState vs3 = VisibilityState::FRUSTUM_CULLED;
    ASSERT_EQ(static_cast<uint8_t>(vs0), 0);
    ASSERT_EQ(static_cast<uint8_t>(vs1), 1);
    ASSERT_EQ(static_cast<uint8_t>(vs2), 2);
    ASSERT_EQ(static_cast<uint8_t>(vs3), 3);

    RegionLoadState rls0 = RegionLoadState::UNLOADED;
    RegionLoadState rls1 = RegionLoadState::LOADING;
    RegionLoadState rls2 = RegionLoadState::LOADED;
    RegionLoadState rls3 = RegionLoadState::UNLOADING;
    ASSERT_EQ(static_cast<uint8_t>(rls0), 0);
    ASSERT_EQ(static_cast<uint8_t>(rls1), 1);
    ASSERT_EQ(static_cast<uint8_t>(rls2), 2);
    ASSERT_EQ(static_cast<uint8_t>(rls3), 3);

    ShapeType st0 = ShapeType::AABB;
    ShapeType st1 = ShapeType::SPHERE;
    ShapeType st2 = ShapeType::OBB;
    ShapeType st3 = ShapeType::CAPSULE;
    ASSERT_EQ(static_cast<uint8_t>(st0), 0);
    ASSERT_EQ(static_cast<uint8_t>(st1), 1);
    ASSERT_EQ(static_cast<uint8_t>(st2), 2);
    ASSERT_EQ(static_cast<uint8_t>(st3), 3);

    ScanMode sm0 = ScanMode::SCAN_ONLY;
    ScanMode sm1 = ScanMode::FULL_ANALYSIS;
    ASSERT_EQ(static_cast<uint8_t>(sm0), 0);
    ASSERT_EQ(static_cast<uint8_t>(sm1), 1);

    AlphaMode am0 = AlphaMode::OPAQUE;
    AlphaMode am1 = AlphaMode::MASK;
    AlphaMode am2 = AlphaMode::BLEND;
    ASSERT_EQ(static_cast<uint8_t>(am0), 0);
    ASSERT_EQ(static_cast<uint8_t>(am1), 1);
    ASSERT_EQ(static_cast<uint8_t>(am2), 2);

    EntityID a = EntityID(5);
    EntityID b = EntityID(5);
    ASSERT_EQ(a, b);

    EntityID c = EntityID(0);
    ASSERT_EQ(c, INVALID_ENTITY_ID);

    std::cout << "  Types tests passed!" << std::endl;
    return true;
}
