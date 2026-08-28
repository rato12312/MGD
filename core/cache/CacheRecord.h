#pragma once

#include <cstdint>
#include "../common/Types.h"

namespace mgd {

constexpr uint32_t MGD_CACHE_FORMAT_VERSION = 1;

struct CacheRecordHeader {
    RecordType type = RecordType::ENTITY;
    uint32_t id = 0;
    uint64_t source_hash = 0;
    uint32_t mgd_format_version = MGD_CACHE_FORMAT_VERSION;
    uint32_t data_size = 0;
};

} // namespace mgd
