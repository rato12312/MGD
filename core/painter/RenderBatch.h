#pragma once

#include "../common/Types.h"
#include <vector>

namespace mgd {

struct RenderBatch {
    MaterialID material;
    MeshID mesh;
    std::vector<uint32_t> entity_indices;
};

} // namespace mgd
