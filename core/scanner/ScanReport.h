#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "GameIdentity.h"
#include "ScanProgress.h"

namespace mgd {

struct ScanReport {
    GameIdentity game;
    ScanProgress stats;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    uint32_t entity_records = 0;
    uint32_t resource_records = 0;
    uint32_t collision_records = 0;
    uint32_t material_records = 0;
    uint32_t texture_records = 0;
    float total_duration_seconds = 0.0f;
};

} // namespace mgd
