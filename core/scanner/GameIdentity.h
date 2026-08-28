#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../common/Types.h"

namespace mgd {

struct GameIdentity {
    std::string game_name = "Unknown";
    std::string series = "Unknown";
    std::string platform = "Unknown";
    std::string source_format = "Unknown";
    std::string game_version = "UNKNOWN";
    std::string content_version = "UNKNOWN";
    std::string scanner_version = "0.1.0";
    std::string mgd_format_version = "1.0";
};

enum class IdentificationConfidence { NONE, LOW, MEDIUM, HIGH, CERTAIN };

struct IdentificationResult {
    GameIdentity identity;
    IdentificationConfidence confidence = IdentificationConfidence::NONE;
    std::vector<std::string> evidence;
    std::vector<std::string> warnings;
};

} // namespace mgd
