#pragma once

#include "Dna.h"
#include "ColorCode.h"
#include "../../common/Types.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mgd {
namespace dna {

// Mapa de pixels: posição dos pixels de cada polígono pré-calculada uma vez.
// Polígono -> posições dos pixels -> código de cor -> Framebuffer.
// Para elementos estáticos evita o trabalho tradicional de rasterização por frame.
struct PixelSpan {
    uint32_t pixel_index = 0;
    uint16_t color_code = 0;
};

class PixelMap {
public:
    // Projeção sintética e estável para pré-cálculo (test scene / bake):
    // mapeia posição do mundo para um bloco de pixels, determinístico.
    void buildStatic(const std::vector<DnaPolygon>& dnas, const XyzIndex& xyz,
                     int fbW, int fbH, int blockW = 4, int blockH = 4) {
        clear();
        fb_w_ = fbW; fb_h_ = fbH;
        for (const auto& d : dnas) {
            Vec3 p = xyz.decode(d.xyz_id);
            int cx = static_cast<int>((p.x * 0.5f + 50.0f)) % fbW;
            if (cx < 0) cx += fbW;
            int cy = static_cast<int>((p.z * 0.5f + 50.0f)) % fbH;
            if (cy < 0) cy += fbH;
            std::vector<PixelSpan> spans;
            spans.reserve(static_cast<size_t>(blockW * blockH));
            for (int oy = 0; oy < blockH; ++oy) {
                for (int ox = 0; ox < blockW; ++ox) {
                    int x = (cx + ox) % fbW;
                    int y = (cy + oy) % fbH;
                    spans.push_back(PixelSpan{static_cast<uint32_t>(y * fbW + x), d.color_code});
                }
            }
            map_[d.polygon_id] = std::move(spans);
        }
    }

    const std::vector<PixelSpan>& get(PolygonID pid) const {
        auto it = map_.find(pid);
        if (it == map_.end()) return kEmpty;
        return it->second;
    }

    size_t polygonCount() const { return map_.size(); }
    void clear() { map_.clear(); fb_w_ = fb_h_ = 0; }

private:
    std::unordered_map<PolygonID, std::vector<PixelSpan>> map_;
    int fb_w_ = 0, fb_h_ = 0;
    static const std::vector<PixelSpan> kEmpty;
};

inline const std::vector<PixelSpan> PixelMap::kEmpty{};

} // namespace dna
} // namespace mgd
