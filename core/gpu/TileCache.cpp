// TileCache Implementation
// Phase 4: Tile Cache with Hash-based lookup

#include "FramebufferOptimizer.h"
#include <algorithm>

namespace mgd {
namespace gpu {

void TileCache::init(uint32_t max_entries) {
    max_entries_ = max_entries;
    cache_.reserve(max_entries * 2);
    current_frame_ = 0;
    hits_ = 0;
    misses_ = 0;
}

void TileCache::shutdown() {
    clear();
}

bool TileCache::lookup(const TileSignature& sig, VkImageView& out_view) {
    auto it = cache_.find(sig);
    if (it != cache_.end() && it->second.valid) {
        out_view = it->second.cached_view;
        it->second.hit_count++;
        it->second.last_used_frame = current_frame_;
        hits_++;
        return true;
    }
    misses_++;
    return false;
}

void TileCache::insert(const TileSignature& sig, VkImageView view, uint32_t frame_idx) {
    // Check if we need to evict
    if (cache_.size() >= max_entries_) {
        evictOld(current_frame_);
    }
    
    TileCacheEntry entry;
    entry.signature_hash = sig.hash();
    entry.cached_view = view;
    entry.last_used_frame = current_frame_;
    entry.hit_count = 0;
    entry.valid = true;
    
    cache_[sig] = std::move(entry);
}

void TileCache::clear() {
    cache_.clear();
    hits_ = 0;
    misses_ = 0;
    current_frame_ = 0;
}

void TileCache::evictOld(uint32_t current_frame) {
    if (cache_.size() < max_entries_) return;
    
    // Find oldest entry
    auto oldest = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.last_used_frame < oldest->second.last_used_frame) {
            oldest = it;
        }
    }
    
    if (oldest != cache_.end()) {
        cache_.erase(oldest);
    }
}

void TileCache::clear() {
    cache_.clear();
    hits_ = 0;
    misses_ = 0;
    current_frame_ = 0;
}

void TileCache::nextFrame() {
    current_frame_++;
}

} // namespace gpu
} // namespace mgd