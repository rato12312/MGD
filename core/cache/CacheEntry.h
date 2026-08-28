#pragma once

#include <cstdint>
#include <vector>
#include "../common/Types.h"

namespace mgd {

struct CacheEntry {
    uint32_t id = 0;
    RecordType type = RecordType::ENTITY;
    uint64_t hash = 0;
    uint64_t timestamp = 0;
    uint32_t version = 0;
    bool valid = false;
    std::vector<uint8_t> data;
};

} // namespace mgd
