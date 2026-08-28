#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/collision/CollisionSystem.h"
#include "core/collision/spatial/GridIndex.h"
#include "core/collision/queries/Raycast.h"

using namespace mgd;

bool run_collision_tests() {
    CollisionSystem cs;

    CollisionShapeData shape1;
    shape1.id = 1;
    shape1.type = ShapeType::AABB;
    shape1.center = Vec3(0, 0, 0);
    shape1.half_extents = Vec3(1, 1, 1);
    cs.addShape(1, shape1);

    CollisionShapeData shape2;
    shape2.id = 2;
    shape2.type = ShapeType::AABB;
    shape2.center = Vec3(5, 0, 0);
    shape2.half_extents = Vec3(1, 1, 1);
    cs.addShape(2, shape2);

    CollisionShapeData retrieved = cs.getShape(1);
    ASSERT_EQ(retrieved.id, static_cast<CollisionID>(1));
    ASSERT_EQ(retrieved.type, ShapeType::AABB);
    ASSERT_NEAR(retrieved.half_extents.x, 1.0f, 0.001f);

    CollisionShapeData retrieved2 = cs.getShape(2);
    ASSERT_EQ(retrieved2.id, static_cast<CollisionID>(2));
    ASSERT_NEAR(retrieved2.center.x, 5.0f, 0.001f);

    CollisionShapeData missing = cs.getShape(999);
    ASSERT_EQ(missing.id, INVALID_COLLISION_ID);

    AABB aabb1 = cs.getWorldAABB(1);
    ASSERT_NEAR(aabb1.min.x, -1.0f, 0.001f);
    ASSERT_NEAR(aabb1.max.x, 1.0f, 0.001f);

    AABB aabb2 = cs.getWorldAABB(2);
    ASSERT_NEAR(aabb2.min.x, 4.0f, 0.001f);
    ASSERT_NEAR(aabb2.max.x, 6.0f, 0.001f);

    Transform t1;
    t1.position = Vec3(10, 0, 0);
    t1.scale = Vec3(1, 1, 1);
    cs.updateTransform(1, t1);
    AABB aabb1Moved = cs.getWorldAABB(1);
    ASSERT_NEAR(aabb1Moved.min.x, 9.0f, 0.001f);
    ASSERT_NEAR(aabb1Moved.max.x, 11.0f, 0.001f);

    std::vector<CollisionID> nearOrigin = cs.queryAABB(AABB(Vec3(-2, -2, -2), Vec3(2, 2, 2)));
    ASSERT_MSG(nearOrigin.size() >= 1, "should find at least 1 shape near origin");
    bool found1 = std::find(nearOrigin.begin(), nearOrigin.end(), static_cast<CollisionID>(1)) != nearOrigin.end();
    ASSERT_MSG(found1, "shape 1 should be found near origin");

    std::vector<CollisionID> nearFive = cs.queryAABB(AABB(Vec3(3, -2, -2), Vec3(7, 2, 2)));
    bool found2 = std::find(nearFive.begin(), nearFive.end(), static_cast<CollisionID>(2)) != nearFive.end();
    ASSERT_MSG(found2, "shape 2 should be found near x=5");

    std::vector<CollisionID> sphereResults = cs.querySphere(Vec3(10, 0, 0), 3.0f);
    ASSERT_MSG(sphereResults.size() >= 1, "sphere query should find shape 1 at x=10");

    cs.removeShape(2);
    CollisionShapeData afterRemove = cs.getShape(2);
    ASSERT_EQ(afterRemove.id, INVALID_COLLISION_ID);

    GridIndex grid(10.0f);
    ASSERT_EQ(grid.size(), static_cast<size_t>(0));

    grid.insert(1, AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1)));
    grid.insert(2, AABB(Vec3(5, -1, -1), Vec3(7, 1, 1)));
    grid.insert(3, AABB(Vec3(50, 50, 50), Vec3(52, 52, 52)));
    ASSERT_EQ(grid.size(), static_cast<size_t>(3));

    std::vector<CollisionID> q1 = grid.query(AABB(Vec3(-2, -2, -2), Vec3(2, 2, 2)));
    bool gFound1 = std::find(q1.begin(), q1.end(), static_cast<CollisionID>(1)) != q1.end();
    ASSERT_MSG(gFound1, "grid should find id 1 near origin");

    std::vector<CollisionID> q2 = grid.query(AABB(Vec3(3, -2, -2), Vec3(8, 2, 2)));
    bool gFound2 = std::find(q2.begin(), q2.end(), static_cast<CollisionID>(2)) != q2.end();
    ASSERT_MSG(gFound2, "grid should find id 2 near x=5");

    std::vector<CollisionID> qFar = grid.query(AABB(Vec3(49, 49, 49), Vec3(53, 53, 53)));
    bool gFound3 = std::find(qFar.begin(), qFar.end(), static_cast<CollisionID>(3)) != qFar.end();
    ASSERT_MSG(gFound3, "grid should find id 3 at (50,50,50)");

    std::vector<CollisionID> qNone = grid.query(AABB(Vec3(100, 100, 100), Vec3(101, 101, 101)));
    ASSERT_EQ(qNone.size(), static_cast<size_t>(0));

    std::vector<CollisionID> pt = grid.queryPoint(Vec3(0.5f, 0.5f, 0.5f));
    bool ptFound = std::find(pt.begin(), pt.end(), static_cast<CollisionID>(1)) != pt.end();
    ASSERT_MSG(ptFound, "queryPoint should find id 1 at (0.5, 0.5, 0.5)");

    grid.remove(1);
    ASSERT_EQ(grid.size(), static_cast<size_t>(2));
    std::vector<CollisionID> qAfterRemove = grid.query(AABB(Vec3(-2, -2, -2), Vec3(2, 2, 2)));
    bool gStillHas1 = std::find(qAfterRemove.begin(), qAfterRemove.end(), static_cast<CollisionID>(1)) != qAfterRemove.end();
    ASSERT_MSG(!gStillHas1, "id 1 should be removed from grid");

    grid.clear();
    ASSERT_EQ(grid.size(), static_cast<size_t>(0));

    CollisionSystem cs2;
    CollisionShapeData rayShape;
    rayShape.id = 10;
    rayShape.type = ShapeType::AABB;
    rayShape.center = Vec3(0, 0, -5);
    rayShape.half_extents = Vec3(1, 1, 1);
    cs2.addShape(10, rayShape);

    RaycastSystem raycaster;
    Ray ray(Vec3(0, 0, 0), Vec3(0, 0, -1));
    RayHit hit = raycaster.raycast(ray, 100.0f, cs2);
    ASSERT_MSG(hit.hit == true, "ray should hit shape at z=-5");
    ASSERT_NEAR(hit.distance, 4.0f, 0.1f);
    ASSERT_NEAR(hit.position.z, -4.0f, 0.5f);

    Ray missRay(Vec3(100, 100, 0), Vec3(0, 0, -1));
    RayHit missHit = raycaster.raycast(missRay, 100.0f, cs2);
    ASSERT_MSG(missHit.hit == false, "ray far away should miss");

    std::cout << "  Collision tests passed!" << std::endl;
    return true;
}
