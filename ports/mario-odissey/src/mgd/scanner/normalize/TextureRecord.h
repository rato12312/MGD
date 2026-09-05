#pragma once

#include <cstdint>
#include <string>
#include "../../common/Types.h"

namespace mgd {

struct TextureRecord {
    TextureID id = 0;
    std::string path;
    int width = 0;
    int height = 0;
    int channels = 4;
    uint64_t hash = 0;
};

} // namespace mgd
