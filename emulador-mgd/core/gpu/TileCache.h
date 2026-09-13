#pragma once

// Tile Cache - Phase 4: Hash-based tile cache with LRU eviction
// Stores rendered tile content for reuse across frames

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <algorithm>

namespace mgd {
namespace gpu {

struct TileSignature {
    uint64_t geometry_hash = 0;
    uint64_t material_hash = 0;
    uint64_t texture_hash = 0;
    uint64_t lighting_hash = 0;
    uint32_t lod_level = 0;
    uint32_t flags = 0;

    bool operator==(const TileSignature& other) const {
        return geometry_hash == other.geometry_hash &&
               material_hash == other.material_hash &&
               texture_hash == other.texture_hash &&
               lighting_hash == other.lighting_hash &&
               lod_level == other.lod_level &&
               flags == other.flags;
    }

    uint64_t hash() const {
        uint64_t h = 14695981039346656037ull;
        auto mix = [](uint64_t h, uint64_t v) {
            h ^= v;
            h *= 1099511628211ull;
            return h;
        };
        h = mix(h, geometry_hash);
        h = mix(h, material_hash);
        h = mix(h, texture_hash);
        h = mix(h, lighting_hash);
        h = mix(h, (uint64_t)lod_level << 32 | flags);
        return h;
    }
};

struct TileCacheEntry {
    uint64_t signature_hash = 0;
    VkImageView cached_view = VK_NULL_HANDLE;
    uint32_t last_used_frame = 0;
    uint32_t hit_count = 0;
    bool valid = false;
};

class TileCache {
public:
    TileCache() = default;
    ~TileCache() = default;

    void init(uint32_t max_entries = 4096);
    void shutdown();

    bool lookup(const TileSignature& sig, VkImageView& out_view);
    void insert(const TileSignature& sig, VkImageView view, uint32_t frame_idx);
    void evictOld(uint32_t current_frame, uint32_t max_age_frames = 60);
    void clear();

    uint32_t hits() const { return hits_; }
    uint32_t misses() const { return misses_; }
    float hitRate() const { return (hits_ + misses_) > 0 ? float(hits_) / float(hits_ + misses_) : 0.0f; }
    void nextFrame() { current_frame_++; }

private:
    struct Hash {
        size_t operator()(const TileSignature& s) const noexcept {
            return std::hash<uint64_t>()(s.hash());
        }
    };

    std::unordered_map<TileSignature, struct TileCacheEntry, Hash> cache_;
    uint32_t max_entries_ = 4096;
    uint32_t current_frame_ = 0;
    uint32_t hits_ = 0;
    uint32_t misses_ = 0;

    void evictOld(uint32_t current_frame, uint32_t max_age_frames);
};

} // namespace gpu
} // namespace mgd