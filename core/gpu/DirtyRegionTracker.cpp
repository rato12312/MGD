// Dirty Region Tracker Implementation
// Phase 2: Dirty Regions tracking with bounding boxes, merging, and cross-over threshold

#include "FramebufferOptimizer.h"
#include <algorithm>
#include <cmath>

namespace mgd {
namespace gpu {

void DirtyRegionTracker::init(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    dirty_rects_.clear();
    merged_rects_.clear();
    stats_ = {};
    current_frame_ = 0;
}

void DirtyRegionTracker::shutdown() {
    dirty_rects_.clear();
    merged_rects_.clear();
    stats_ = {};
}

void DirtyRegionTracker::addDirtyRect(const DirtyRect& rect) {
    if (!rect.empty() && rect.w > 0 && rect.h > 0) {
        // Clamp to framebuffer bounds
        DirtyRect clamped = rect;
        if (clamped.x >= width_) clamped.w = 0;
        else if (clamped.x + clamped.w > width_) clamped.w = width_ - clamped.x;
        if (clamped.y >= height_) clamped.h = 0;
        else if (clamped.y + clamped.h > height_) clamped.h = height_ - clamped.y;
        
        if (!clamped.empty()) {
            dirty_rects_.push_back(clamped);
        }
    }
}

void DirtyRegionTracker::addDirtyRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    addDirtyRect({x, y, w, h});
}

void DirtyRegionTracker::mergeOverlapping() {
    if (dirty_rects_.size() <= 1) {
        merged_rects_ = dirty_rects_;
        return;
    }
    
    // Sort by y, then x
    std::sort(dirty_rects_.begin(), dirty_rects_.end(), 
        [](const DirtyRect& a, const DirtyRect& b) {
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        });
    
    merged_rects_.clear();
    merged_rects_.push_back(dirty_rects_[0]);
    
    for (size_t i = 1; i < dirty_rects_.size(); ++i) {
        DirtyRect& last = merged_rects_.back();
        const DirtyRect& current = dirty_rects_[i];
        
        // Check if rectangles overlap or are adjacent (within 1 pixel)
        bool overlap_x = !(last.x + last.w <= dirty_rects_[i].x || 
                          dirty_rects_[i].x + dirty_rects_[i].w <= last.x);
        bool overlap_y = !(last.y + last.h <= dirty_rects_[i].y || 
                          dirty_rects_[i].y + dirty_rects_[i].h <= last.y);
        bool adjacent_x = (last.x + last.w == dirty_rects_[i].x) || 
                         (dirty_rects_[i].x + dirty_rects_[i].w == last.x);
        bool adjacent_y = (last.y + last.h == dirty_rects_[i].y) || 
                         (dirty_rects_[i].y + dirty_rects_[i].h == last.y);
        
        if ((overlap_x && overlap_y) || (overlap_x && adjacent_y) || (adjacent_x && overlap_y)) {
            // Merge rectangles
            uint32_t min_x = std::min(last.x, dirty_rects_[i].x);
            uint32_t min_y = std::min(last.y, dirty_rects_[i].y);
            uint32_t max_x = std::max(last.x + last.w, dirty_rects_[i].x + dirty_rects_[i].w);
            uint32_t max_y = std::max(last.y + last.h, dirty_rects_[i].y + dirty_rects_[i].h);
            
            last.x = min_x;
            last.y = min_y;
            last.w = max_x - min_x;
            last.h = max_y - min_y;
        } else {
            merged_rects_.push_back(dirty_rects_[i]);
        }
    }
    
    dirty_rects_ = merged_rects_;
}

void DirtyRegionTracker::clear() {
    dirty_rects_.clear();
    merged_rects_.clear();
    stats_ = {};
}

void DirtyRegionTracker::updateStats() {
    stats_.dirty_rect_count = static_cast<uint32_t>(merged_rects_.size());
    stats_.dirty_pixel_count = 0;
    for (const auto& rect : merged_rects_) {
        stats_.dirty_pixel_count += rect.area();
    }
    stats_.total_pixels = width_ * height_;
    stats_.dirty_ratio = stats_.total_pixels > 0 ? 
        float(stats_.dirty_pixel_count) / float(stats_.total_pixels) : 0.0f;
}

void DirtyRegionTracker::setCrossOverThreshold(float ratio) {
    cross_over_threshold_ = std::clamp(ratio, 0.0f, 1.0f);
}

bool DirtyRegionTracker::shouldDoFullFrame() const {
    return stats_.dirty_ratio > cross_over_threshold_;
}

void DirtyRegionTracker::init(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    clear();
}

void DirtyRegionTracker::shutdown() {
    clear();
}

void DirtyRegionTracker::clear() {
    dirty_rects_.clear();
    merged_rects_.clear();
    stats_ = {};
    current_frame_ = 0;
}

void DirtyRegionTracker::addDirtyRect(const DirtyRect& rect) {
    DirtyRect clamped = rect;
    if (clamped.x >= width_) clamped.w = 0;
    else if (clamped.x + clamped.w > width_) clamped.w = width_ - clamped.x;
    if (clamped.y >= height_) clamped.h = 0;
    else if (clamped.y + clamped.h > height_) clamped.h = height_ - clamped.y;
    
    if (!clamped.empty()) {
        dirty_rects_.push_back(clamped);
    }
}

void DirtyRegionTracker::addDirtyRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    addDirtyRect({x, y, w, h});
}

void DirtyRegionTracker::mergeOverlapping() {
    if (dirty_rects_.size() <= 1) {
        merged_rects_ = dirty_rects_;
        return;
    }
    
    std::sort(dirty_rects_.begin(), dirty_rects_.end(), 
        [](const DirtyRect& a, const DirtyRect& b) {
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        });
    
    merged_rects_.clear();
    merged_rects_.push_back(dirty_rects_[0]);
    
    for (size_t i = 1; i < dirty_rects_.size(); ++i) {
        DirtyRect& last = merged_rects_.back();
        const DirtyRect& current = dirty_rects_[i];
        
        bool overlap_x = !(last.x + last.w <= dirty_rects_[i].x || 
                          dirty_rects_[i].x + dirty_rects_[i].w <= last.x);
        bool overlap_y = !(last.y + last.h <= dirty_rects_[i].y || 
                          dirty_rects_[i].y + dirty_rects_[i].h <= last.y);
        bool adjacent_x = (last.x + last.w == dirty_rects_[i].x) || 
                         (dirty_rects_[i].x + dirty_rects_[i].w == last.x);
        bool adjacent_y = (last.y + last.h == dirty_rects_[i].y) || 
                         (dirty_rects_[i].y + dirty_rects_[i].h == last.y);
        
        if ((overlap_x && overlap_y) || (overlap_x && adjacent_y) || (adjacent_x && overlap_y)) {
            uint32_t min_x = std::min(last.x, dirty_rects_[i].x);
            uint32_t min_y = std::min(last.y, dirty_rects_[i].y);
            uint32_t max_x = std::max(last.x + last.w, dirty_rects_[i].x + dirty_rects_[i].w);
            uint32_t max_y = std::max(last.y + last.h, dirty_rects_[i].y + dirty_rects_[i].h);
            
            last.x = min_x;
            last.y = min_y;
            last.w = max_x - min_x;
            last.h = max_y - min_y;
        } else {
            merged_rects_.push_back(dirty_rects_[i]);
        }
    }
    
    dirty_rects_ = merged_rects_;
}

void DirtyRegionTracker::updateStats() {
    stats_.dirty_rect_count = static_cast<uint32_t>(merged_rects_.size());
    stats_.dirty_pixel_count = 0;
    for (const auto& rect : merged_rects_) {
        stats_.dirty_pixel_count += rect.area();
    }
    stats_.total_pixels = width_ * height_;
    stats_.dirty_ratio = stats_.total_pixels > 0 ? 
        float(stats_.dirty_pixel_count) / float(stats_.total_pixels) : 0.0f;
}

} // namespace gpu
} // namespace mgd