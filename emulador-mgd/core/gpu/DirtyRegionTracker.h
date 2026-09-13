#pragma once

// Dirty Region Tracker - Phase 2: Dirty Regions tracking with bounding boxes, merging, and cross-over threshold

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace mgd {
namespace gpu {

struct DirtyRect {
    uint32_t x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w == 0 || h == 0; }
    bool overlaps(const DirtyRect& other) const {
        return !(x + w <= other.x || other.x + other.w <= x ||
                 y + h <= other.y || other.y + other.h <= y);
    }
    bool adjacent(const DirtyRect& other) const {
        return (x + w == other.x || other.x + other.w == x) &&
               !(y + h <= other.y || other.y + other.h <= y);
    }
};

struct DirtyRegionStats {
    uint32_t dirty_rect_count = 0;
    uint64_t dirty_pixel_count = 0;
    uint64_t total_pixels = 0;
    float dirty_ratio = 0.0f;
    double process_time_ms = 0.0;
};

class DirtyRegionTracker {
public:
    DirtyRegionTracker() = default;
    ~DirtyRegionTracker() = default;

    void init(uint32_t width, uint32_t height);
    void shutdown();

    void addDirtyRect(const DirtyRect& rect);
    void addDirtyRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void mergeOverlapping();
    void clear();

    const std::vector<DirtyRect>& getDirtyRects() const { return dirty_rects_; }
    const std::vector<DirtyRect>& getMergedRects() const { return merged_rects_; }
    const DirtyRegionStats& getStats() const { return stats_; }

    void setCrossOverThreshold(float ratio) { cross_over_threshold_ = ratio; }
    bool shouldDoFullFrame() const { return stats_.dirty_ratio > cross_over_threshold_; }

private:
    uint32_t width_ = 0, height_ = 0;
    std::vector<DirtyRect> dirty_rects_;
    std::vector<DirtyRect> merged_rects_;
    DirtyRegionStats stats_;
    float cross_over_threshold_ = 0.5f; // 50% = full frame
    uint32_t current_frame_ = 0;

    void updateStats();
    void mergeRects();
};

} // namespace gpu
} // namespace mgd