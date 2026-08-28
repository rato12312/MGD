#include "ScanProgress.h"

namespace mgd {

float ScanProgress::elapsedSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - start_time).count();
}

float ScanProgress::estimatedRemainingSeconds() const {
    if (files_analyzed == 0 || total_files_discovered == 0) {
        return 0.0f;
    }
    float elapsed = elapsedSeconds();
    float rate = static_cast<float>(files_analyzed) / elapsed;
    uint32_t remaining = total_files_discovered - files_analyzed;
    if (rate <= 0.0f) {
        return 0.0f;
    }
    return static_cast<float>(remaining) / rate;
}

void ScanProgress::updateProgress() {
    if (total_files_discovered > 0) {
        progress_percentage = (static_cast<float>(files_analyzed) /
                               static_cast<float>(total_files_discovered)) * 100.0f;
    }
}

} // namespace mgd
