#include <iostream>
#include <string>
#include <cmath>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)
#define ASSERT_EQ(a, b) ASSERT_MSG((a) == (b), std::string(#a) + " != " + #b)
#define ASSERT_NEAR(a, b, eps) ASSERT_MSG(std::abs((a) - (b)) < (eps), std::string(#a) + " != " + #b)

#include "core/mental_map/Entity.h"

using namespace mgd;

bool run_entity_tests() {
    MentalEntity e;
    ASSERT_EQ(e.id, INVALID_ENTITY_ID);
    ASSERT_EQ(e.state, EntityState::ACTIVE);
    ASSERT_EQ(e.visibility, VisibilityState::UNCHECKED);
    ASSERT_EQ(e.parent_id, INVALID_ENTITY_ID);
    ASSERT_EQ(e.flags, static_cast<uint32_t>(0));
    ASSERT_EQ(e.region_id, INVALID_REGION_ID);
    ASSERT_EQ(e.collision_id, INVALID_COLLISION_ID);
    ASSERT_EQ(e.visual_ref, static_cast<uint32_t>(0));
    ASSERT_EQ(e.resource_id, static_cast<ResourceID>(0));
    ASSERT_EQ(e.children.size(), static_cast<size_t>(0));

    ASSERT_EQ(e.transform.position.x, 0.0f);
    ASSERT_EQ(e.transform.position.y, 0.0f);
    ASSERT_EQ(e.transform.position.z, 0.0f);

    ASSERT_EQ(e.transform.scale.x, 1.0f);
    ASSERT_EQ(e.transform.scale.y, 1.0f);
    ASSERT_EQ(e.transform.scale.z, 1.0f);

    ASSERT_EQ(e.transform.rotation_euler.x, 0.0f);
    ASSERT_EQ(e.transform.rotation_euler.y, 0.0f);
    ASSERT_EQ(e.transform.rotation_euler.z, 0.0f);

    AABB defaultBox;
    Vec3 center = defaultBox.center();
    Vec3 extents = defaultBox.extents();
    ASSERT_NEAR(center.x, 0.0f, 0.001f);
    ASSERT_NEAR(center.y, 0.0f, 0.001f);
    ASSERT_NEAR(center.z, 0.0f, 0.001f);
    ASSERT_NEAR(extents.x, 0.0f, 0.001f);
    ASSERT_NEAR(extents.y, 0.0f, 0.001f);
    ASSERT_NEAR(extents.z, 0.0f, 0.001f);

    ASSERT_MSG(e.isVisible() == false, "UNCHECKED should not be visible");
    ASSERT_MSG(e.isLoaded() == true, "ACTIVE should be loaded");

    e.transform.position = Vec3(1.0f, 2.0f, 3.0f);
    ASSERT_NEAR(e.transform.position.x, 1.0f, 0.001f);
    ASSERT_NEAR(e.transform.position.y, 2.0f, 0.001f);
    ASSERT_NEAR(e.transform.position.z, 3.0f, 0.001f);

    e.bounds.aabb = AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1));
    Vec3 bcenter = e.bounds.aabb.center();
    Vec3 bextents = e.bounds.aabb.extents();
    ASSERT_NEAR(bcenter.x, 0.0f, 0.001f);
    ASSERT_NEAR(bcenter.y, 0.0f, 0.001f);
    ASSERT_NEAR(bcenter.z, 0.0f, 0.001f);
    ASSERT_NEAR(bextents.x, 1.0f, 0.001f);
    ASSERT_NEAR(bextents.y, 1.0f, 0.001f);
    ASSERT_NEAR(bextents.z, 1.0f, 0.001f);

    e.state = EntityState::INACTIVE;
    ASSERT_MSG(e.isLoaded() == false, "INACTIVE should not be loaded");

    e.state = EntityState::DIRTY;
    ASSERT_MSG(e.isLoaded() == true, "DIRTY should be loaded");

    e.state = EntityState::HIDDEN;
    ASSERT_MSG(e.isLoaded() == false, "HIDDEN should not be loaded");

    e.state = EntityState::DISABLED;
    ASSERT_MSG(e.isLoaded() == false, "DISABLED should not be loaded");

    e.state = EntityState::ACTIVE;
    e.visibility = VisibilityState::VISIBLE;
    ASSERT_MSG(e.isVisible() == true, "VISIBLE should be visible");

    e.visibility = VisibilityState::OCCLUDED;
    ASSERT_MSG(e.isVisible() == false, "OCCLUDED should not be visible");

    e.visibility = VisibilityState::FRUSTUM_CULLED;
    ASSERT_MSG(e.isVisible() == false, "FRUSTUM_CULLED should not be visible");

    MentalEntity e2;
    e2.id = 42;
    e2.flags = 0xFF;
    e2.resource_id = 7;
    e2.collision_id = 3;
    e2.region_id = 2;
    ASSERT_EQ(e2.id, static_cast<EntityID>(42));
    ASSERT_EQ(e2.flags, static_cast<uint32_t>(0xFF));
    ASSERT_EQ(e2.resource_id, static_cast<ResourceID>(7));
    ASSERT_EQ(e2.collision_id, static_cast<CollisionID>(3));
    ASSERT_EQ(e2.region_id, static_cast<RegionID>(2));

    std::cout << "  Entity tests passed!" << std::endl;
    return true;
}
