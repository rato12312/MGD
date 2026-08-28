#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace mgd {

struct ScanProgress {
    uint32_t total_files_discovered = 0;
    uint32_t files_scanned = 0;
    uint32_t files_analyzed = 0;
    uint32_t cache_hits = 0;
    uint32_t cache_misses = 0;
    uint32_t errors = 0;
    uint32_t warnings = 0;
    uint64_t bytes_processed = 0;
    std::string current_file;
    float progress_percentage = 0.0f;
    std::chrono::steady_clock::time_point start_time;

    float elapsedSeconds() const;
    float estimatedRemainingSeconds() const;
    void updateProgress();
};

} // namespace mgd
