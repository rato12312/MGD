#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/mental_map/MentalMap.h"

using namespace mgd;

bool run_mental_map_tests() {
    MentalMap map;
    ASSERT_EQ(map.entityCount(), static_cast<size_t>(0));

    MentalEntity e1;
    e1.id = 1;
    e1.state = EntityState::ACTIVE;
    e1.transform.position = Vec3(0, 0, 0);
    EntityID id1 = map.addEntity(e1);
    ASSERT_EQ(id1, static_cast<EntityID>(1));
    ASSERT_EQ(map.entityCount(), static_cast<size_t>(1));

    MentalEntity e2;
    e2.id = 2;
    e2.state = EntityState::ACTIVE;
    e2.transform.position = Vec3(10, 0, 0);
    EntityID id2 = map.addEntity(e2);
    ASSERT_EQ(id2, static_cast<EntityID>(2));
    ASSERT_EQ(map.entityCount(), static_cast<size_t>(2));

    MentalEntity e3;
    e3.id = 3;
    e3.state = EntityState::ACTIVE;
    e3.transform.position = Vec3(20, 0, 0);
    EntityID id3 = map.addEntity(e3);
    ASSERT_EQ(id3, static_cast<EntityID>(3));
    ASSERT_EQ(map.entityCount(), static_cast<size_t>(3));

    auto active = map.getActiveEntities();
    ASSERT_EQ(active.size(), static_cast<size_t>(3));

    ASSERT_MSG(map.getEntity(1).has_value(), "entity 1 should exist");
    ASSERT_MSG(map.getEntity(2).has_value(), "entity 2 should exist");
    ASSERT_MSG(map.getEntity(3).has_value(), "entity 3 should exist");
    ASSERT_MSG(!map.getEntity(99).has_value(), "entity 99 should not exist");

    auto& ent1 = map.getEntity(1)->get();
    ASSERT_NEAR(ent1.transform.position.x, 0.0f, 0.001f);

    map.setState(2, EntityState::INACTIVE);
    active = map.getActiveEntities();
    ASSERT_EQ(active.size(), static_cast<size_t>(2));

    ASSERT_MSG(map.removeEntity(2), "remove entity 2 should succeed");
    ASSERT_EQ(map.entityCount(), static_cast<size_t>(2));
    ASSERT_MSG(!map.getEntity(2).has_value(), "entity 2 should not exist after removal");

    ASSERT_MSG(!map.removeEntity(99), "remove non-existent should fail");

    active = map.getActiveEntities();
    ASSERT_EQ(active.size(), static_cast<size_t>(2));

    Region r1;
    r1.id = 1;
    r1.bounds = AABB(Vec3(-50, -50, -50), Vec3(50, 50, 50));
    r1.load_state = RegionLoadState::LOADED;
    map.addRegion(r1);

    Region r2;
    r2.id = 2;
    r2.bounds = AABB(Vec3(50, -50, -50), Vec3(150, 50, 50));
    r2.load_state = RegionLoadState::LOADED;
    map.addRegion(r2);

    ASSERT_MSG(map.getRegion(1).has_value(), "region 1 should exist");
    ASSERT_MSG(map.getRegion(2).has_value(), "region 2 should exist");
    ASSERT_MSG(!map.getRegion(99).has_value(), "region 99 should not exist");

    auto loadedIds = map.getLoadedRegionIDs();
    ASSERT_EQ(loadedIds.size(), static_cast<size_t>(2));

    map.setRegion(1, 1);
    auto entitiesInRegion = map.getEntitiesInRegion(1);
    ASSERT_EQ(entitiesInRegion.size(), static_cast<size_t>(1));
    ASSERT_EQ(entitiesInRegion[0], static_cast<EntityID>(1));

    auto inRange = map.getEntitiesInRange(Vec3(5, 0, 0), 15.0f);
    ASSERT_EQ(inRange.size(), static_cast<size_t>(2));

    auto farAway = map.getEntitiesInRange(Vec3(1000, 0, 0), 1.0f);
    ASSERT_EQ(farAway.size(), static_cast<size_t>(0));

    map.setParent(1, 3);
    auto parent = map.getParent(1);
    ASSERT_MSG(parent.has_value(), "entity 1 should have parent");
    ASSERT_EQ(parent.value(), static_cast<EntityID>(3));

    auto children = map.getChildren(3);
    ASSERT_EQ(children.size(), static_cast<size_t>(1));
    ASSERT_EQ(children[0], static_cast<EntityID>(1));

    auto roots = map.getRootEntities();
    ASSERT_EQ(roots.size(), static_cast<size_t>(1));
    ASSERT_EQ(roots[0], static_cast<EntityID>(3));

    map.removeParent(1);
    auto noParent = map.getParent(1);
    ASSERT_MSG(!noParent.has_value(), "entity 1 should have no parent after removal");

    map.setVisibility(1, VisibilityState::VISIBLE);
    auto& ent = map.getEntity(1)->get();
    ASSERT_EQ(ent.visibility, VisibilityState::VISIBLE);

    map.setTransform(1, Transform{Vec3(5, 5, 5), Vec3(0, 0, 0), Vec3(1, 1, 1)});
    auto& entMoved = map.getEntity(1)->get();
    ASSERT_NEAR(entMoved.transform.position.x, 5.0f, 0.001f);
    ASSERT_NEAR(entMoved.transform.position.y, 5.0f, 0.001f);
    ASSERT_NEAR(entMoved.transform.position.z, 5.0f, 0.001f);

    map.clear();
    ASSERT_EQ(map.entityCount(), static_cast<size_t>(0));

    std::cout << "  MentalMap tests passed!" << std::endl;
    return true;
}
