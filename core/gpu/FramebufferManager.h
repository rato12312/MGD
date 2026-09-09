#pragma once

// Framebuffer Manager — gerencia reuso de frames + dirty regions + motion vectors
// O rascunho (Mali) escreve em framebuffer 288p; Painter faz upscale 720p.
// Este manager:
// - Mantém history buffer (frame N-1 reconstruído)
// - Rastreia dirty rects do rascunho (onde o Mali escreveu)
// - Calcula motion vectors por objeto (para reprojeção temporal)
// - Fornece dirty regions para Painter compute shader

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <memory>
#include <cstdint>
#include <cstring>

namespace mgd {
namespace gpu {

struct Rect2D {
    int x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
    bool intersects(const Rect2D& other) const {
        return x < other.x + other.w && x + w > other.x &&
               y < other.y + other.h && y + h > other.y;
    }
    Rect2D united(const Rect2D& other) const {
        int minx = std::min(x, other.x);
        int miny = std::min(y, other.y);
        int maxx = std::max(x + w, other.x + other.w);
        int maxy = std::max(y + h, other.y + other.h);
        return {minx, miny, maxx - minx, maxy - miny};
    }
};

struct MotionVector {
    float u = 0.0f, v = 0.0f; // motion em pixels (rascunho 288p)
    uint32_t object_id = 0;
};

struct FrameHistory {
    std::vector<uint8_t> color;    // 720p RGBA8 reconstruído (frame N-1)
    std::vector<uint16_t> depth;   // 720p depth reconstruído
    std::vector<MotionVector> mv;  // motion vectors 288p
    std::vector<uint32_t> obj_id;  // 288p object_id buffer
    int w_720 = 1280, h_720 = 720;
    int w_288 = 512, h_288 = 288;
    uint64_t frame_index = 0;
    bool valid = false;
};

class FramebufferManager {
public:
    FramebufferManager(int rough_w = 512, int rough_h = 288, int final_w = 1280, int final_h = 720)
        : rough_w_(rough_w), rough_h_(rough_h), final_w_(final_w), final_h_(final_h) {
        history_[0] = std::make_unique<FrameHistory>();
        history_[1] = std::make_unique<FrameHistory>();
        history_[0]->w_720 = final_w; history_[0]->h_720 = final_h;
        history_[0]->w_288 = rough_w; history_[0]->h_288 = rough_h;
        history_[1]->w_720 = final_w; history_[1]->h_720 = final_h;
        history_[1]->w_288 = rough_w; history_[1]->h_288 = rough_h;
        allocateBuffers();
    }

    // Inicia novo frame: troca history buffers
    void beginFrame(uint64_t frame_index) {
        current_ = 1 - current_;
        auto& hist = *history_[current_];
        hist.frame_index = frame_index;
        hist.valid = false;
        dirty_rects_.clear();
        dirty_rects_.reserve(64);
    }

    // Registra região suja no rascunho (coordenadas 288p)
    void addDirtyRect(const Rect2D& r) {
        Rect2D clamped = r;
        clamped.x = std::max(0, std::min(rough_w_, clamped.x));
        clamped.y = std::max(0, std::min(rough_h_, clamped.y));
        clamped.w = std::max(0, std::min(rough_w_ - clamped.x, clamped.w));
        clamped.h = std::max(0, std::min(rough_h_ - clamped.y, clamped.h));
        if (!clamped.empty()) dirty_rects_.push_back(clamped);
    }

    // Mescla dirty rects sobrepostos
    void mergeDirtyRects() {
        if (dirty_rects_.size() <= 1) return;
        std::sort(dirty_rects_.begin(), dirty_rects_.end(),
            [](const Rect2D& a, const Rect2D& b) { return a.y < b.y || (a.y == b.y && a.x < b.x); });
        std::vector<Rect2D> merged;
        merged.push_back(dirty_rects_[0]);
        for (size_t i = 1; i < dirty_rects_.size(); ++i) {
            Rect2D& last = merged.back();
            if (last.intersects(dirty_rects_[i]) || 
                (last.x + last.w >= dirty_rects_[i].x && last.y == dirty_rects_[i].y)) {
                last = last.united(dirty_rects_[i]);
            } else {
                merged.push_back(dirty_rects_[i]);
            }
        }
        dirty_rects_.swap(merged);
    }

    // Finaliza frame: computa motion vectors, marca history como válida
    void endFrame(const std::vector<uint32_t>& obj_id_288, 
                  const std::vector<uint16_t>& depth_288,
                  const std::vector<Vec3>& obj_positions) {
        auto& hist = *history_[current_];
        hist.valid = true;
        
        // Copia object_id e depth do rascunho para history
        if (!obj_id_288.empty()) {
            hist.obj_id.assign(obj_id_288.begin(), obj_id_288.end());
        }
        if (!depth_288.empty()) {
            hist.depth.assign(depth_288.begin(), depth_288.end());
        }

        // Computa motion vectors simples: diferença de posição dos objetos
        if (!obj_positions.empty() && prev_obj_positions_.size() == obj_positions.size()) {
            hist.mv.resize(rough_w_ * rough_h_);
            // Simplificado: motion vector uniforme por frame baseado em camera movement
            // Real: por objeto via object_id buffer
            for (int y = 0; y < rough_h_; ++y) {
                for (int x = 0; x < rough_w_; ++x) {
                    int idx = y * rough_w_ + x;
                    uint32_t obj = hist.obj_id[idx];
                    if (obj < prev_obj_positions_.size() && obj < obj_positions.size()) {
                        Vec3 prev = prev_obj_positions_[obj];
                        Vec3 cur = obj_positions[obj];
                        Vec3 diff = cur - prev;
                        // Projeta para screen space (simplificado)
                        hist.mv[idx] = {diff.x * 100.0f, diff.y * 100.0f, obj};
                    }
                }
            }
        }
        prev_obj_positions_ = obj_positions;
    }

    // Obtém dirty rects mesclados para Painter
    const std::vector<Rect2D>& getDirtyRects() const { return dirty_rects_; }

    // Obtém history buffer anterior para reprojeção
    const FrameHistory* getPrevHistory() const {
        return history_[1 - current_]->valid ? history_[1 - current_].get() : nullptr;
    }

    const FrameHistory* getCurrentHistory() const { return history_[current_].get(); }

    // Retorna buffers para Painter compute shader
    const uint8_t* getPrevColor() const { 
        auto* h = getPrevHistory(); 
        return h && !h->color.empty() ? h->color.data() : nullptr; 
    }
    const uint16_t* getPrevDepth() const { 
        auto* h = getPrevHistory(); 
        return h && !h->depth.empty() ? h->depth.data() : nullptr; 
    }
    const MotionVector* getMotionVectors() const { 
        auto* h = getPrevHistory(); 
        return h && !h->mv.empty() ? h->mv.data() : nullptr; 
    }
    const uint32_t* getPrevObjId() const { 
        auto* h = getPrevHistory(); 
        return h && !h->obj_id.empty() ? h->obj_id.data() : nullptr; 
    }

    int roughWidth() const { return rough_w_; }
    int roughHeight() const { return rough_h_; }
    int finalWidth() const { return final_w_; }
    int finalHeight() const { return final_h_; }

private:
    void allocateBuffers() {
        for (auto& h : history_) {
            h->color.resize(final_w_ * final_h_ * 4); // RGBA8
            h->depth.resize(final_w_ * final_h_);
            h->mv.resize(rough_w_ * rough_h_);
            h->obj_id.resize(rough_w_ * rough_h_);
        }
    }

    int rough_w_, rough_h_;
    int final_w_, final_h_;
    int current_ = 0;
    std::array<std::unique_ptr<FrameHistory>, 2> history_;
    std::vector<Rect2D> dirty_rects_;
    std::vector<Vec3> prev_obj_positions_;
};

} // namespace gpu
} // namespace mgd