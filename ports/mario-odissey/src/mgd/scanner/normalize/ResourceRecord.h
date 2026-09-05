#pragma once

#include <cstdint>
#include <string>
#include "../../common/Types.h"
#include "../../common/AABB.h"

namespace mgd {

struct ResourceRecord {
    ResourceID id = 0;
    std::string name;
    std::string path;
    uint64_t hash = 0;
    AABB bounds;
    uint32_t triangle_count = 0;
    uint32_t vertex_count = 0;
};

} // namespace mgd
