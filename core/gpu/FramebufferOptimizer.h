#pragma once

// MGD Framebuffer Optimizer (MFO)
// Fase 1: Framebuffer Básico (atual + anterior + diff pixel count)
// Fase 2: Dirty Regions (bounding boxes, união, invalidação)
// Fase 3: Tiles (divisão, estado por tile, processamento só tiles alterados)
// Fase 4: Cache (hash/signature, Pixel Cache, cache hit/miss, invalidação seletiva)
// Fase 5: Integração MGD (Visibility → MFO, DNA → MFO, Pixel Map → MFO, Seed Predictor → prefetch)

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <array>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <cstring>
#include <algorithm>

#include "VulkanContext.h"
#include "FramebufferManager.h"

namespace mgd {
namespace gpu {

// ============================================================================
// FASE 1: Framebuffer Básico
// ============================================================================

struct FramebufferState {
    VkImageView color_view = VK_NULL_HANDLE;
    VkImageView depth_view = VK_NULL_HANDLE;
    VkExtent2D extent{};
    VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_format = VK_FORMAT_D16_UNORM;
    uint64_t frame_index = 0;
    double timestamp_ms = 0.0;
};

struct FrameDiffStats {
    uint64_t total_pixels = 0;
    uint64_t changed_pixels = 0;
    uint64_t unchanged_pixels = 0;
    float change_ratio = 0.0f;
    double diff_time_ms = 0.0;
};

class FramebufferBasic {
public:
    FramebufferBasic() = default;
    ~FramebufferBasic() = default;

    bool init(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM);
    void shutdown();

    // Inicia novo frame
    void beginFrame(uint64_t frame_index);

    // Compara framebuffer atual com anterior
    FrameDiffStats compareAndSwap(VkCommandBuffer cmd, VkImageView current_color, VkImageView current_depth);

    // Acessa framebuffer anterior para reuso
    const FramebufferState& getPreviousFrame() const { return prev_frame_; }
    const FramebufferState& getCurrentFrame() const { return curr_frame_; }
    const FrameDiffStats& getLastDiffStats() const { return last_diff_; }

    VkExtent2D getExtent() const { return {width_, height_}; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }

private:
    VulkanContext* ctx_ = nullptr;
    uint32_t width_ = 0, height_ = 0;
    VkFormat color_format_ = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_format_ = VK_FORMAT_D16_UNORM;

    FramebufferState curr_frame_;
    FramebufferState prev_frame_;
    FrameDiffStats last_diff_;

    // Resources
    VkImage current_color_img_ = VK_NULL_HANDLE;
    VkImageView current_color_view_ = VK_NULL_HANDLE;
    VkImageView current_depth_view_ = VK_NULL_HANDLE;
    VkDeviceMemory current_color_mem_ = VK_NULL_HANDLE;
    VkDeviceMemory current_depth_mem_ = VK_NULL_HANDLE;

    VkImage prev_color_img_ = VK_NULL_HANDLE;
    VkImageView prev_color_view_ = VK_NULL_HANDLE;
    VkImageView prev_depth_view_ = VK_NULL_HANDLE;
    VkDeviceMemory prev_color_mem_ = VK_NULL_HANDLE;
    VkDeviceMemory prev_depth_mem_ = VK_NULL_HANDLE;

    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;
    VkCommandBuffer cmd_buffer_ = VK_NULL_HANDLE;

    bool initialized_ = false;

    bool createRenderPass();
    bool createFramebufferResources(uint32_t width, uint32_t height);
    void destroyFrameResources();
    void copyFramebuffer(VkCommandBuffer cmd, VkImageView src_color, VkImageView src_depth, VkImageView dst_color, VkImageView dst_depth);
    FrameDiffStats compareFramebuffers(VkCommandBuffer cmd, VkImageView curr_color, VkImageView curr_depth, VkImageView prev_color, VkImageView prev_depth);
};


// ============================================================================
// FASE 2: Dirty Regions
// ============================================================================

struct DirtyRect {
    uint32_t x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w == 0 || h == 0; }
    bool intersects(const DirtyRect& other) const {
        return !(x + w <= other.x || other.x + other.w <= x ||
                 y + h <= other.y || other.y + other.h <= y);
    }
    DirtyRect united(const DirtyRect& other) const {
        uint32_t min_x = std::min(x, other.x);
        uint32_t min_y = std::min(y, other.y);
        uint32_t max_x = std::max(x + w, other.x + other.w);
        uint32_t max_y = std::max(y + h, other.y + other.h);
        return {std::min(x, other.x), std::min(y, other.y), max_x - min_x, max_y - min_y};
    }
    uint64_t area() const { return uint64_t(w) * h; }
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

    // Adiciona região suja
    void addDirtyRect(const DirtyRect& rect);
    void addDirtyRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

    // Mescla retângulos sobrepostos/adjacentes
    void mergeOverlapping();

    // Limpa para próximo frame
    void clear();

    const std::vector<DirtyRect>& getDirtyRects() const { return dirty_rects_; }
    const DirtyRegionStats& getStats() const { return stats_; }

    // Cross-over threshold: se dirty > threshold, faz full frame
    void setCrossOverThreshold(float ratio) { cross_over_threshold_ = ratio; }
    bool shouldDoFullFrame() const { return stats_.dirty_ratio > cross_over_threshold_; }

private:
    uint32_t width_ = 0, height_ = 0;
    std::vector<DirtyRect> dirty_rects_;
    std::vector<DirtyRect> merged_rects_;
    DirtyRegionStats stats_;
    float cross_over_threshold_ = 0.5f; // 50% = full frame

    void updateStats();
    void mergeRects();
};


// ============================================================================
// FASE 3: Tiles
// ============================================================================

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
    uint64_t hash = 0;        // Hash do conteúdo do tile
    uint32_t dirty_frame = 0; // Último frame em que ficou dirty
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

    // Marca tile como dirty
    void markDirty(uint32_t tile_x, uint32_t tile_y, uint64_t content_hash = 0);
    void markClean(uint32_t tile_x, uint32_t tile_y);
    void markRebuild(uint32_t tile_x, uint32_t tile_y);
    void markReuse(uint32_t tile_x, uint32_t tile_y);

    // Processa apenas tiles dirty
    template<typename Func>
    void forEachDirtyTile(Func&& func) {
        for (auto& tile : tiles_) {
            if (tile.state == TileState::DIRTY || tile.state == TileState::REBUILD) {
                func(tile);
            }
        }
    }

    // Marca tile para reuso (hash igual)
    void markReuseByHash(uint32_t tile_x, uint32_t tile_y, uint64_t content_hash);

    const TileInfo& getTile(uint32_t tx, uint32_t ty) const {
        return tiles_[ty * tiles_x_ + tx];
    }

    TileInfo& getTile(uint32_t tx, uint32_t tile_y) {
        return tiles_[tile_y * tiles_x_ + tile_x];
    }

    const TileGridStats& getStats() const { return stats_; }
    uint32_t getTilesX() const { return tiles_x_; }
    uint32_t getTilesY() const { return tiles_y_; }
    uint32_t tileWidth() const { return tile_w_; }
    uint32_t tileHeight() const { return tile_h_; }

    void nextFrame();

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


// ============================================================================
// FASE 4: Cache + Hash
// ============================================================================

struct TileSignature {
    uint64_t geometry_hash = 0;      // Hash da geometria
    uint64_t material_hash = 0;      // Hash do material
    uint64_t texture_hash = 0;       // Hash das texturas
    uint64_t lighting_hash = 0;      // Hash da iluminação
    uint32_t lod_level = 0;          // LOD level
    uint32_t flags = 0;              // Flags extras

    bool operator==(const TileSignature& other) const {
        return geometry_hash == other.geometry_hash &&
               material_hash == other.material_hash &&
               texture_hash == other.texture_hash &&
               lighting_hash == other.lighting_hash &&
               lod_level == other.lod_level &&
               flags == other.flags;
    }

    uint64_t hash() const {
        // FNV-1a hash combiner
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
    VkImageView cached_view = VK_NULL_HANDLE; // Rendered tile content
    uint32_t last_used_frame = 0;
    uint32_t hit_count = 0;
    bool valid = false;
};

class TileCache {
public:
    TileCache() = default;
    ~TileCache() { clear(); }

    void init(uint32_t max_entries = 4096);
    void shutdown();

    // Busca entrada no cache
    bool lookup(const TileSignature& sig, VkImageView& out_view) {
        auto it = cache_.find(sig);
        if (it != cache_.end() && it->second.valid) {
            out_view = it->second.cached_view;
            it->second.hit_count++;
            it->second.last_used_frame = current_frame_;
            return true;
        }
        return false;
    }

    // Insere/atualiza entrada
    void insert(const TileSignature& sig, VkImageView view, uint32_t frame_idx);

    // Invalida entradas antigas (LRU + max entries)
    void evictOld(uint32_t current_frame, uint32_t max_age_frames = 60);

    void clear();

    uint32_t hits() const { return hits_; }
    uint32_t misses() const { return misses_; }
    float hitRate() const { return (hits_ + misses_) > 0 ? float(hits_) / (hits_ + misses_) : 0.0f; }

    void nextFrame() { current_frame_++; }

private:
    struct Hash {
        size_t operator()(const TileSignature& s) const noexcept {
            return std::hash<uint64_t>()(s.hash());
        }
    };

    std::unordered_map<TileSignature, TileCacheEntry, Hash> cache_;
    uint32_t current_frame_ = 0;
    uint32_t hits_ = 0;
    uint32_t misses_ = 0;
    uint32_t max_entries_ = 4096;
};


// ============================================================================
// FASE 5: Integração MGD - MFO Integration
// ============================================================================

struct MFOStats {
    uint64_t frame_index = 0;
    uint64_t total_pixels = 0;
    uint64_t changed_pixels = 0;
    uint64_t reused_pixels = 0;
    uint32_t dirty_tiles = 0;
    uint32_t total_tiles = 0;
    uint32_t cache_hits = 0;
    uint32_t cache_misses = 0;
    uint64_t bytes_written = 0;
    uint64_t bytes_reused = 0;
    double mfo_time_ms = 0.0;
    double frame_time_ms = 0.0;

    float reuseRatio() const {
        return total_pixels > 0 ? float(reused_pixels) / float(total_pixels) : 0.0f;
    }
    float cacheHitRate() const {
        return (cache_hits + cache_misses) > 0 ? float(cache_hits) / (cache_hits + cache_misses) : 0.0f;
    }
};

struct MFOConfig {
    uint32_t tile_w = 64;
    uint32_t tile_h = 64;
    float cross_over_threshold = 0.5f; // 50% dirty = full frame
    bool enable_tile_cache = true;
    uint32_t max_tile_cache_entries = 4096;
    bool enable_occlusion_culling = true;
    float cross_over_threshold = 0.5f;
};

struct MFOInput {
    // From Mental Map / Visibility
    std::vector<uint32_t> visible_polygon_ids;
    std::vector<uint32_t> visible_tile_indices;
    std::vector<uint32_t> dirty_object_ids;
    
    // From DNA / Pixel Map
    std::vector<uint64_t> polygon_hashes; // Hash por polígono
    
    // Camera info
    struct CameraInfo {
        float x, y, z;
        float pitch, yaw, roll;
        float fov;
        float aspect;
    } camera;
    
    // Frame info
    uint64_t frame_index = 0;
    float delta_time = 0.0f;
};

struct MFOOutput {
    std::vector<uint32_t> dirty_tile_indices;
    std::vector<uint32_t> rebuild_tile_indices;
    std::vector<uint32_t> reuse_tile_indices;
    uint32_t total_tiles = 0;
    uint32_t dirty_tile_count = 0;
    uint32_t reused_tile_count = 0;
    
    // Para o Painter
    std::vector<uint32_t> dirty_tile_indices_for_painter;
};

class MFOManager {
public:
    MFOManager() = default;
    ~MFOManager() { shutdown(); }

    bool init(VulkanContext* ctx, uint32_t width, uint32_t height, const MFOConfig& config = {});
    void shutdown();

    // Main entry point
    bool processFrame(const MFOInput& input, MFOOutput& output);

    // Accessors
    const MFOStats& getStats() const { return stats_; }
    const MFOConfig& getConfig() const { return config_; }
    void setConfig(const MFOConfig& cfg) { config_ = cfg; }

    // Framebuffer access
    VkImageView getCurrentColorView() const { return current_color_view_; }
    VkImageView getCurrentDepthView() const { return current_depth_view_; }
    VkExtent2D getExtent() const { return {width_, height_}; }

    // Metrics
    const MFOStats& getStats() const { return stats_; }
    float getReuseRatio() const { return stats_.reuseRatio(); }
    float getCacheHitRate() const { return stats_.cacheHitRate(); }

private:
    MFOConfig config_;
    MFOStats stats_;

    VulkanContext* ctx_ = nullptr;
    uint32_t width_ = 0, height_ = 0;

    // Phase 1: Framebuffer básico
    std::unique_ptr<FramebufferBasic> framebuffer_basic_;

    // Phase 2: Dirty regions
    std::unique_ptr<DirtyRegionTracker> dirty_tracker_;

    // Phase 3: Tile grid
    std::unique_ptr<TileGrid> tile_grid_;

    // Phase 4: Tile cache
    std::unique_ptr<TileCache> tile_cache_;

    // Framebuffer images
    VkImageView current_color_view_ = VK_NULL_HANDLE;
    VkImageView current_depth_view_ = VK_NULL_HANDLE;
    VkImageView prev_color_view_ = VK_NULL_HANDLE;
    VkImageView prev_depth_view_ = VK_NULL_HANDLE;

    // Stats
    MFOStats stats_;

    // Config
    MFOConfig config_;

    // Private methods
    bool initPhase1_FramebufferBasic();
    bool initPhase2_DirtyRegions();
    bool initPhase3_Tiles();
    bool initPhase4_Cache();
    bool initPhase5_Integration();

    void processPhase1_FramebufferBasic(const MFOInput& input);
    void processPhase2_DirtyRegions(const MFOInput& input);
    void processPhase3_Tiles(const MFOInput& input);
    void processPhase4_Cache(const MFOInput& input);
    void processPhase5_Integration(const MFOInput& input, MFOOutput& output);
    void updateStats();
    void swapBuffers();
    void updateTileStates();
    void computeDirtyTiles(const MFOInput& input);
    void updateTileHashes();
    void evictOldCacheEntries();
};

} // namespace gpu
} // namespace mgd