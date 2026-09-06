#pragma once

#include "ColorCode.h"
#include "../../painter/framebuffer/Framebuffer.h"
#include <cstdint>
#include <vector>

namespace mgd {
namespace dna {

// Cache incremental do framebuffer: o MGD não reescreve a tela inteira quando
// só uma parte mudou. Guarda o código de cor por pixel e só escreve os alterados.
// Ex.: Pixel 1542 -> código 27, Pixel 1543 -> código 27, Pixel 1601 -> código 31.
class IncrementalPixelCache {
public:
    IncrementalPixelCache() = default;
    IncrementalPixelCache(int w, int h) { resize(w, h); }

    void resize(int w, int h) {
        w_ = w; h_ = h;
        codes_.assign(static_cast<size_t>(w) * h, kEmptyCode);
        dirty_.assign(static_cast<size_t>(w) * h, 0);
        dirty_list_.clear();
        dirty_list_.reserve(static_cast<size_t>(w) * h / 16);
        dirty_count_ = 0;
    }

    // Define código do pixel; marca dirty só se mudou (e registra na lista).
    void setPixelCode(uint32_t pixelIndex, uint16_t code) {
        if (pixelIndex >= codes_.size()) return;
        if (codes_[pixelIndex] != code) {
            codes_[pixelIndex] = code;
            if (!dirty_[pixelIndex]) {
                dirty_[pixelIndex] = 1;
                dirty_list_.push_back(pixelIndex);
                dirty_count_++;
            }
        }
    }

    // Escreve só pixels alterados no framebuffer via LUT (sem recalcular cor).
    // Anda só pela lista de sujos: O(dirty), não O(W*H).
    uint32_t flush(Framebuffer& fb) {
        uint32_t written = 0;
        if (fb.width() != w_ || fb.height() != h_) return 0;
        for (uint32_t i : dirty_list_) {
            dirty_[i] = 0;
            int x = static_cast<int>(i % static_cast<uint32_t>(w_));
            int y = static_cast<int>(i / static_cast<uint32_t>(w_));
            fb.setPixel(x, y, ColorCode::decode(codes_[i]));
            written++;
        }
        dirty_list_.clear();
        dirty_count_ = 0;
        return written;
    }

    uint32_t dirtyCount() const { return dirty_count_; }
    size_t pixelCount() const { return codes_.size(); }
    void clearDirty() {
        for (uint32_t i : dirty_list_) dirty_[i] = 0;
        dirty_list_.clear();
        dirty_count_ = 0;
    }

private:
    static constexpr uint16_t kEmptyCode = 0xFFFF;
    int w_ = 0, h_ = 0;
    std::vector<uint16_t> codes_;
    std::vector<char> dirty_;
    std::vector<uint32_t> dirty_list_; // só índices sujos: flush O(dirty)
    uint32_t dirty_count_ = 0;
};

} // namespace dna
} // namespace mgd
