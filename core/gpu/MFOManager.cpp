// MFOManager Implementation
// Framebuffer Optimizer Manager - Integrates all phases

#include "FramebufferOptimizer.h"
#include "VulkanContext.h"
#include "FramebufferManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace mgd {
namespace gpu {

bool MFOManager::init(VulkanContext* ctx, uint32_t width, uint32_t height, const MFOConfig& config) {
    ctx_ = ctx;
    width_ = width;
    height_ = height;
    config_ = config;
    stats_ = {};
    
    // Initialize phases in order
    if (!initPhase1_FramebufferBasic()) return false;
    if (!initPhase2_DirtyRegions()) return false;
    if (!initPhase3_Tiles()) return false;
    if (!initPhase4_Cache()) return false;
    if (!initPhase5_Integration()) return false;
    
    return true;
}

void MFOManager::shutdown() {
    asset_pipeline_.reset();
    painter_.reset();
    fb_mgr_.reset();
    tile_cache_.reset();
    tile_grid_.reset();
    dirty_tracker_.reset();
    framebuffer_basic_.reset();
    ctx_ = nullptr;
    fb_mgr_ = nullptr;
}

bool MFOManager::initPhase1_FramebufferBasic() {
    framebuffer_basic_ = std::make_unique<FramebufferBasic>();
    return framebuffer_basic_->init(ctx_, width_, height_, VK_FORMAT_R8G8B8A8_UNORM);
}

bool MFOManager::initPhase2_DirtyRegions() {
    dirty_tracker_ = std::make_unique<DirtyRegionTracker>();
    dirty_tracker_->init(width_, height_);
    dirty_tracker_->setCrossOverThreshold(config_.cross_over_threshold);
    return true;
}

bool MFOManager::initPhase3_Tiles() {
    tile_grid_ = std::make_unique<TileGrid>();
    tile_grid_->init(width_, height_, config_.tile_w, config_.tile_h);
    tile_grid_->setCrossOverThreshold(config_.cross_over_threshold);
    return true;
}

bool MFOManager::initPhase4_Cache() {
    if (!config_.enable_tile_cache) return true;
    
    tile_cache_ = std::make_unique<TileCache>();
    tile_cache_->init(config_.max_tile_cache_entries);
    return true;
}

bool MFOManager::initPhase5_Integration() {
    // Initialize painter compute
    painter_ = std::make_unique<PainterCompute>();
    if (!painter_->init(ctx_, fb_mgr_)) return false;
    
    // Initialize asset pipeline
    asset_pipeline_ = std::make_unique<AssetPipeline>();
    // asset_pipeline_->init(ctx_, registry, infector); // Would need registry/infector
    
    return true;
}

bool MFOManager::processFrame(const MFOInput& input, MFOOutput& output) {
    auto frame_start = std::chrono::high_resolution_clock::now();
    
    stats_.frame_index = input.frame_index;
    
    // Phase 1: Framebuffer Basic
    processPhase1_FramebufferBasic(input);
    
    // Phase 2: Dirty Regions
    processPhase2_DirtyRegions(input);
    
    // Phase 3: Tile Processing
    processPhase3_Tiles(input);
    
    // Phase 4: Cache
    processPhase4_Cache(input);
    
    // Phase 5: Integration
    processPhase5_Integration(input, output);
    
    // Update stats
    auto frame_end = std::chrono::high_resolution_clock::now();
    auto frame_start = std::chrono::high_resolution_clock::now() - std::chrono::milliseconds(16); // Approximate
    stats_.frame_time_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
    updateStats();
    
    output.dirty_tile_indices = tile_grid_->getDirtyTileIndices();
    output.rebuild_tile_indices = tile_grid_->getRebuildTileIndices();
    output.reuse_tile_indices = tile_grid_->getReuseTileIndices();
    output.total_tiles = tile_grid_->getTotalTiles();
    output.dirty_tile_count = tile_grid_->getDirtyTileCount();
    output.reused_tile_count = tile_grid_->getReuseTileCount();
    
    return true;
}

void MFOManager::processPhase1_FramebufferBasic(const MFOInput& input) {
    // Framebuffer basic handling is done in VulkanGpuExecutor
    // Here we just track stats
    stats_.frame_index = input.frame_index;
}

void MFOManager::processPhase2_DirtyRegions(const MFOInput& input) {
    dirty_tracker_->clear();
    
    // Add dirty regions from input
    for (uint32_t obj_id : input.dirty_object_ids) {
        // Would query object bounds and add to dirty tracker
        // For now, mark whole frame as potentially dirty
    }
    
    // Add camera movement as dirty region
    if (input.camera.x != last_camera_.x || 
        input.camera.y != last_camera_.y || 
        input.camera.z != last_camera_.z) {
        // Camera moved - mark affected regions
        dirty_tracker_->addDirtyRect(0, 0, width_, height_);
    }
    last_camera_ = input.camera;
    
    dirty_tracker_->mergeOverlapping();
    dirty_tracker_->updateStats();
}

void MFOManager::processPhase3_Tiles(const MFOInput& input) {
    tile_grid_->nextFrame();
    tile_grid_->setCamera(&camera_); // Would need camera reference
    
    // Get dirty regions from tracker
    const auto& dirty_rects = dirty_tracker_->getDirtyRects();
    
    // Mark affected tiles as dirty
    for (const auto& rect : dirty_tracker_->getDirtyRects()) {
        tile_grid_->markDirtyRegion(rect.x, rect.y, rect.w, rect.h);
    }
    
    // Mark dirty objects' tiles
    for (uint32_t obj_id : input.dirty_object_ids) {
        // Would query object bounds and mark tiles
    }
    
    // Run tile grid query with camera
    auto result = tile_grid_->query(input.frame_index);
    // Process results...
    
    // Update tile states based on cache
    updateTileStates();
}

void MFOManager::processPhase4_Cache(const MFOInput& input) {
    if (!config_.enable_tile_cache) return;
    
    updateTileHashes();
    evictOldCacheEntries();
}

void MFOManager::processPhase5_Integration(const MFOInput& input, MFOOutput& output) {
    // Prepare output
    output.dirty_tile_indices = tile_grid_->getDirtyTileIndices();
    output.rebuild_tile_indices = tile_grid_->getRebuildTileIndices();
    output.reuse_tile_indices = tile_grid_->getReuseTileIndices();
    output.total_tiles = tile_grid_->getTotalTiles();
    output.dirty_tile_count = tile_grid_->getDirtyTileCount();
    output.reused_tile_count = tile_grid_->getReuseTileCount();
    
    // Prepare dirty tile indices for painter
    output.dirty_tile_indices_for_painter = tile_grid_->getDirtyTileIndices();
}

void MFOManager::updateStats() {
    auto& tile_stats = tile_grid_->getStats();
    stats_.total_tiles = tile_stats.total_tiles;
    stats_.dirty_tiles = tile_stats.dirty_tiles;
    stats_.reused_tiles = tile_grid_->getReuseTileCount();
    stats_.cache_hits = tile_cache_ ? tile_cache_->hits() : 0;
    stats_.cache_misses = tile_cache_ ? tile_cache_->misses() : 0;
}

void MFOManager::swapBuffers() {
    // Swap framebuffers for next frame
}

void MFOManager::updateTileStates() {
    // Update tile states based on cache lookup
    for (uint32_t ty = 0; ty < tile_grid_->getTilesY(); ++ty) {
        for (uint32_t tx = 0; tx < tile_grid_->getTilesX(); ++tx) {
            auto& tile = tile_grid_->getTile(tx, ty);
            if (tile.state == TileState::DIRTY) {
                // Check cache
                TileSignature sig;
                sig.geometry_hash = tile.hash;
                sig.lod_level = tile.lod_level;
                
                VkImageView cached_view;
                if (tile_cache_ && tile_cache_->lookup(sig, cached_view)) {
                    tile.state = TileState::REUSE;
                    tile_cache_->hits();
                } else {
                    tile.state = TileState::REBUILD;
                    tile.rebuild_count++;
                }
            }
        }
    }
}

void MFOManager::computeDirtyTiles(const MFOInput& input) {
    // Compute which tiles are dirty based on input
    tile_grid_->clearDirty();
    
    // Mark tiles from dirty regions
    for (const auto& rect : dirty_tracker_->getDirtyRects()) {
        tile_grid_->markDirtyRegion(rect.x, rect.y, rect.w, rect.h);
    }
    
    // Mark tiles from dirty objects
    for (uint32_t obj_id : input.dirty_object_ids) {
        // Would query object bounds and mark tiles
    }
}

void MFOManager::updateTileHashes() {
    // Update tile hashes based on content
    for (uint32_t ty = 0; ty < tile_grid_->getTilesY(); ++ty) {
        for (uint32_t tx = 0; tx < tile_grid_->getTilesX(); ++tx) {
            auto& tile = tile_grid_->getTile(tx, ty);
            if (tile.state == TileState::DIRTY || tile.state == TileState::REBUILD) {
                // Compute hash from tile content
                TileSignature sig;
                sig.geometry_hash = tile.hash;
                sig.lod_level = tile.lod_level;
                tile.signature_hash = sig.hash();
            }
        }
    }
}

void MFOManager::evictOldCacheEntries() {
    if (tile_cache_) {
        tile_cache_->evictOld(stats_.frame_index);
    }
}

void MFOManager::updateTileHashes() {
    // Update hashes for dirty tiles
    for (uint32_t ty = 0; ty < tile_grid_->getTilesY(); ++ty) {
        for (uint32_t tx = 0; tx < tile_grid_->getTilesX(); ++tx) {
            auto& tile = tile_grid_->getTile(tx, ty);
            if (tile.state == TileState::DIRTY || tile.state == TileState::REBUILD) {
                TileSignature sig;
                sig.geometry_hash = tile.hash;
                sig.lod_level = tile.lod_level;
                tile.signature_hash = sig.hash();
            }
        }
    }
}

void MFOManager::evictOldCacheEntries() {
    if (tile_cache_) {
        tile_cache_->evictOld(stats_.frame_index);
    }
}

void MFOManager::updateStats() {
    if (tile_grid_) {
        auto& tile_stats = tile_grid_->getStats();
        stats_.total_tiles = tile_stats.total_tiles;
        stats_.dirty_tiles = tile_stats.dirty_tiles;
        stats_.reused_tiles = tile_grid_->getReuseTileCount();
    }
    
    if (tile_cache_) {
        stats_.cache_hits = tile_cache_->hits();
        stats_.cache_misses = tile_cache_->misses();
    }
}

void MFOManager::swapBuffers() {
    // Swap framebuffers for next frame
}

void MFOManager::shutdown() {
    asset_pipeline_.reset();
    painter_.reset();
    fb_mgr_.reset();
    tile_cache_.reset();
    tile_grid_.reset();
    dirty_tracker_.reset();
    framebuffer_basic_.reset();
    ctx_ = nullptr;
    fb_mgr_ = nullptr;
}

} // namespace gpu
} // namespace mgd