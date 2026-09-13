#pragma once

// Tile Grid - Phase 3: Tile-based framebuffer management
// Divides framebuffer into tiles for granular reuse/recache

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace mgd {
namespace gpu {

enum class TileState {
    CLEAN = 0,
    DIRTY = 1,
    REBUILD = 2,
    REUSE = 3
};

struct TileInfo {
    uint32_t x = 0, y = 0;
    uint32_t w = 0, h = 0;
    TileState state = TileState::CLEAN;
    uint64_t hash = 0;
    uint32_t dirty_frame = 0;
    uint32_t rebuild_count = 0;
};

struct TileGridStats {
    uint32_t total_tiles = 0;
    uint32_t dirty_tiles = 0;
    uint32_t clean_tiles = 0;
    uint32_t rebuild_tiles = 0;
    uint32_t reuse_tiles = 0;
    float dirty_ratio = 0.0f;
};

class TileGrid {
public:
    TileGrid() = default;
    ~TileGrid() = default;

    void init(uint32_t width, uint32_t height, uint32_t tile_w = 64, uint32_t tile_h = 64);
    void shutdown();

    void markDirty(uint32_t tile_x, uint32_t tile_y, uint64_t content_hash = 0);
    void markClean(uint32_t tile_x, uint32_t tile_y);
    void markRebuild(uint32_t tile_x, uint32_t tile_y);
    void markReuse(uint32_t tile_x, uint32_t tile_y);
    void markReuseByHash(uint32_t tile_x, uint32_t tile_y, uint64_t content_hash);

    template<typename Func>
    void forEachDirtyTile(Func&& func) {
        for (auto& tile : tiles_) {
            if (tile.state == TileState::DIRTY || tile.state == TileState::REBUILD) {
                func(tile);
            }
        }
    }

    void markReuseByHash(uint32_t tile_x, uint32_t tile_y, uint64_t content_hash);

    const TileInfo& getTile(uint32_t tx, uint32_t ty) const {
        return tiles_[ty * tiles_x_ + tx];
    }
    TileInfo& getTile(uint32_t tx, uint32_t ty) {
        return tiles_[ty * tiles_x_ + tx];
    }

    const TileGridStats& getStats() const { return stats_; }
    uint32_t getTilesX() const { return tiles_x_; }
    uint32_t getTilesY() const { return tiles_y_; }
    uint32_t tileWidth() const { return tile_w_; }
    uint32_t tileHeight() const { return tile_h_; }

    void nextFrame();

    uint32_t getTilesX() const { return tiles_x_; }
    uint32_t getTilesY() const { return tiles_y_; }
    uint32_t tileWidth() const { return tile_w_; }
    uint32_t tileHeight() const { return tile_h_; }

private:
    uint32_t width_ = 0, height_ = 0;
    uint32_t tile_w_ = 64, tile_h_ = 64;
    uint32_t tiles_x_ = 0, tiles_y_ = 0;
    std::vector<TileInfo> tiles_;
    uint32_t current_frame_ = 0;
    TileGridStats stats_;

    void updateStats();
    void markNeighborsDirty(uint32_t tx, uint32_t ty);
};

} // namespace gpu
} // namespace mgd