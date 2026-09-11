// TileGrid Implementation
// Phase 3: Tile-based framebuffer management

#include "FramebufferOptimizer.h"
#include <algorithm>

namespace mgd {
namespace gpu {

void TileGrid::init(uint32_t width, uint32_t height, uint32_t tile_w, uint32_t tile_h) {
    width_ = width;
    height_ = height;
    tile_w_ = tile_w;
    tile_h_ = tile_h;
    tiles_x_ = (width + tile_w - 1) / tile_w;
    tiles_y_ = (height + tile_h - 1) / tile_h;
    
    tiles_.resize(tiles_x_ * tiles_y_);
    for (auto& tile : tiles_) {
        tile.state = TileState::CLEAN;
        tile.hash = 0;
        tile.dirty_frame = 0;
        tile.rebuild_count = 0;
    }
    
    stats_ = {};
}

void TileGrid::shutdown() {
    tiles_.clear();
    stats_ = {};
}

void TileGrid::markDirty(uint32_t tile_x, uint32_t tile_y, uint64_t content_hash) {
    if (tile_x >= tiles_x_ || tile_y >= tiles_y_) return;
    
    TileInfo& tile = tiles_[tile_y * tiles_x_ + tile_x];
    if (tile.state != TileState::DIRTY && tile.state != TileState::REBUILD) {
        tile.state = TileState::DIRTY;
        tile.dirty_frame = current_frame_;
        if (content_hash != 0) {
            tile.hash = content_hash;
        }
    }
}

void TileGrid::markClean(uint32_t tile_x, uint32_t tile_y) {
    if (tile_x >= tiles_x_ || tile_y >= tiles_y_) return;
    TileInfo& tile = tiles_[tile_y * tiles_x_ + tile_x];
    tile.state = TileState::CLEAN;
}

void TileGrid::markRebuild(uint32_t tile_x, uint32_t tile_y) {
    if (tile_x >= tiles_x_ || tile_y >= tiles_y_) return;
    TileInfo& tile = tiles_[tile_y * tiles_x_ + tile_x];
    tile.state = TileState::REBUILD;
    tile.rebuild_count++;
}

void TileGrid::markReuse(uint32_t tile_x, uint32_t tile_y) {
    if (tile_x >= tiles_x_ || tile_y >= tiles_y_) return;
    TileInfo& tile = tiles_[tile_y * tiles_x_ + tile_x];
    tile.state = TileState::REUSE;
}

void TileGrid::markReuseByHash(uint32_t tile_x, uint32_t tile_y, uint64_t content_hash) {
    if (tile_x >= tiles_x_ || tile_y >= tiles_y_) return;
    TileInfo& tile = tiles_[tile_y * tiles_x_ + tile_x];
    if (tile.hash == content_hash) {
        tile.state = TileState::REUSE;
    } else {
        tile.state = TileState::DIRTY;
        tile.hash = content_hash;
    }
}

void TileGrid::nextFrame() {
    current_frame_++;
    updateStats();
}

void TileGrid::updateStats() {
    stats_.total_tiles = tiles_.size();
    stats_.dirty_tiles = 0;
    stats_.clean_tiles = 0;
    stats_.rebuild_tiles = 0;
    stats_.reuse_tiles = 0;
    
    for (const auto& tile : tiles_) {
        switch (tile.state) {
            case TileState::DIRTY: stats_.dirty_tiles++; break;
            case TileState::CLEAN: stats_.clean_tiles++; break;
            case TileState::REBUILD: stats_.rebuild_tiles++; break;
            case TileState::REUSE: stats_.reuse_tiles++; break;
        }
    }
    stats_.dirty_ratio = tiles_.empty() ? 0.0f : 
        float(stats_.dirty_tiles) / float(tiles_.size());
}

void TileGrid::markNeighborsDirty(uint32_t tx, uint32_t ty) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = int(tx) + dx;
            int ny = int(ty) + dy;
            if (nx >= 0 && nx < int(tiles_x_) && ny >= 0 && ny < int(tiles_y_)) {
                tiles_[ny * tiles_x_ + nx].state = TileState::DIRTY;
            }
        }
    }
}

void TileGrid::markDirtyRegion(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t start_tx = std::min(x / tile_w_, tiles_x_);
    uint32_t end_tx = std::min((x + w + tile_w_ - 1) / tile_w_, tiles_x_);
    uint32_t start_ty = std::min(y / tile_h_, tiles_y_);
    uint32_t end_ty = std::min((y + h + tile_h_ - 1) / tile_h_, tiles_y_);
    
    for (uint32_t ty = start_ty; ty < end_ty; ++ty) {
        for (uint32_t tx = start_tx; tx < end_tx; ++tx) {
            tiles_[ty * tiles_x_ + tx].state = TileState::DIRTY;
        }
    }
}

void TileGrid::clear() {
    for (auto& tile : tiles_) {
        tile.state = TileState::CLEAN;
        tile.hash = 0;
        tile.dirty_frame = 0;
        tile.rebuild_count = 0;
    }
    stats_ = {};
}

void TileGrid::updateStats() {
    stats_.total_tiles = tiles_.size();
    stats_.dirty_tiles = 0;
    stats_.clean_tiles = 0;
    stats_.rebuild_tiles = 0;
    stats_.reuse_tiles = 0;
    
    for (const auto& tile : tiles_) {
        switch (tile.state) {
            case TileState::DIRTY: stats_.dirty_tiles++; break;
            case TileState::CLEAN: stats_.clean_tiles++; break;
            case TileState::REBUILD: stats_.rebuild_tiles++; break;
            case TileState::REUSE: stats_.reuse_tiles++; break;
        }
    }
    stats_.dirty_ratio = tiles_.empty() ? 0.0f : 
        float(stats_.dirty_tiles) / float(tiles_.size());
}

void TileGrid::markNeighborsDirty(uint32_t tx, uint32_t ty) {
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = int(tx) + dx;
            int ny = int(ty) + dy;
            if (nx >= 0 && nx < int(tiles_x_) && ny >= 0 && ny < int(tiles_y_)) {
                tiles_[ny * tiles_x_ + nx].state = TileState::DIRTY;
            }
        }
    }
}

} // namespace gpu
} // namespace mgd