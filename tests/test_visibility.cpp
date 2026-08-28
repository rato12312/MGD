#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/visibility/BasicVisibility.h"
#include "core/mental_map/MentalMap.h"
#include "core/camera/Camera.h"
#include "core/collision/CollisionSystem.h"

using namespace mgd;

bool run_visibility_tests() {
    MentalMap map;
    CollisionSystem collision;
    Camera camera;

    camera.setPosition(Vec3(0, 0, 10));
    camera.setOrientation(Vec3(0, 0, 0));

    MentalEntity frontEnt;
    frontEnt.id = 1;
    frontEnt.collision_id = 1;
    frontEnt.state = EntityState::ACTIVE;
    frontEnt.transform.position = Vec3(0, 0, 20);
    frontEnt.bounds.aabb = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
    map.addEntity(frontEnt);

    CollisionShapeData frontShape;
    frontShape.id = 1;
    frontShape.type = ShapeType::AABB;
    frontShape.center = Vec3(0, 0, 20);
    frontShape.half_extents = Vec3(1, 1, 1);
    collision.addShape(1, frontShape);

    MentalEntity behindEnt;
    behindEnt.id = 2;
    behindEnt.collision_id = 2;
    behindEnt.state = EntityState::ACTIVE;
    behindEnt.transform.position = Vec3(0, 0, -5);
    behindEnt.bounds.aabb = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
    map.addEntity(behindEnt);

    CollisionShapeData behindShape;
    behindShape.id = 2;
    behindShape.type = ShapeType::AABB;
    behindShape.center = Vec3(0, 0, -5);
    behindShape.half_extents = Vec3(1, 1, 1);
    collision.addShape(2, behindShape);

    MentalEntity farEnt;
    farEnt.id = 3;
    farEnt.collision_id = 3;
    farEnt.state = EntityState::ACTIVE;
    farEnt.transform.position = Vec3(0, 0, 500);
    farEnt.bounds.aabb = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
    map.addEntity(farEnt);

    CollisionShapeData farShape;
    farShape.id = 3;
    farShape.type = ShapeType::AABB;
    farShape.center = Vec3(0, 0, 500);
    farShape.half_extents = Vec3(1, 1, 1);
    collision.addShape(3, farShape);

    MentalEntity inactiveEnt;
    inactiveEnt.id = 4;
    inactiveEnt.collision_id = 4;
    inactiveEnt.state = EntityState::INACTIVE;
    inactiveEnt.transform.position = Vec3(0, 0, 30);
    inactiveEnt.bounds.aabb = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
    map.addEntity(inactiveEnt);

    CollisionShapeData inactiveShape;
    inactiveShape.id = 4;
    inactiveShape.type = ShapeType::AABB;
    inactiveShape.center = Vec3(0, 0, 30);
    inactiveShape.half_extents = Vec3(1, 1, 1);
    collision.addShape(4, inactiveShape);

    BasicVisibility vis;
    VisibleSet result = vis.compute(map, camera, collision);

    ASSERT_MSG(result.total_considered > 0, "should consider at least some entities");
    ASSERT_MSG(result.total_visible <= result.total_considered, "visible should be <= considered");

    bool foundFront = false;
    for (const auto& ve : result.entities) {
        if (ve.id == 1) foundFront = true;
    }
    ASSERT_MSG(foundFront, "entity in front of camera should be visible");

    bool foundBehind = false;
    for (const auto& ve : result.entities) {
        if (ve.id == 2) foundBehind = true;
    }
    ASSERT_MSG(!foundBehind, "entity behind camera should NOT be visible");

    bool foundInactive = false;
    for (const auto& ve : result.entities) {
        if (ve.id == 4) foundInactive = true;
    }
    ASSERT_MSG(!foundInactive, "INACTIVE entity should not be visible");

    for (const auto& ve : result.entities) {
        ASSERT_MSG(ve.distance_to_camera >= 0.0f, "distance should be non-negative");
    }

    MentalMap emptyMap;
    CollisionSystem emptyCollision;
    VisibleSet emptyResult = vis.compute(emptyMap, camera, emptyCollision);
    ASSERT_EQ(emptyResult.entities.size(), static_cast<size_t>(0));
    ASSERT_EQ(emptyResult.total_visible, static_cast<size_t>(0));

    std::cout << "  Visibility tests passed!" << std::endl;
    return true;
}
