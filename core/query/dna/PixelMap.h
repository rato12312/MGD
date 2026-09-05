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
    // Níveis de detalhe pré-preparados por polígono (não reconstruídos):
    // LOD 0 (longe, compacto 2x2), LOD 1 (médio 4x4), LOD 2 (perto, 8x8).
    static constexpr int LOD_COUNT = 3;

    // Projeção sintética e estável para pré-cálculo (test scene / bake):
    // mapeia posição do mundo para blocos de pixels, determinístico.
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
            // LODs: 0 = compacto (canto 2x2 do bloco), 2 = detalhado (bloco dobrado)
            LodSpans lods;
            lods[1] = map_[d.polygon_id];
            lods[0].reserve(4);
            for (int oy = 0; oy < 2 && oy < blockH; ++oy)
                for (int ox = 0; ox < 2 && ox < blockW; ++ox)
                    lods[0].push_back(PixelSpan{static_cast<uint32_t>(((cy + oy) % fbH) * fbW + ((cx + ox) % fbW)), d.color_code});
            lods[2].reserve(static_cast<size_t>(blockW * 2 * (blockH * 2)));
            for (int oy = 0; oy < blockH * 2; ++oy)
                for (int ox = 0; ox < blockW * 2; ++ox)
                    lods[2].push_back(PixelSpan{static_cast<uint32_t>(((cy + oy) % fbH) * fbW + ((cx + ox) % fbW)), d.color_code});
            lod_map_[d.polygon_id] = std::move(lods);
        }
    }

    const std::vector<PixelSpan>& get(PolygonID pid) const {
        auto it = map_.find(pid);
        if (it == map_.end()) return kEmpty;
        return it->second;
    }

    // Distância seleciona representação preparada (não reconstrói).
    const std::vector<PixelSpan>& getLod(PolygonID pid, int lod) const {
        auto it = lod_map_.find(pid);
        if (it == lod_map_.end()) return kEmpty;
        if (lod < 0) lod = 0;
        if (lod >= LOD_COUNT) lod = LOD_COUNT - 1;
        return it->second[lod];
    }

    // Seleção por distância: longe LOD 0, médio LOD 1, perto LOD 2.
    static int selectLod(float distance, float nearDist = 20.0f, float farDist = 60.0f) {
        if (distance >= farDist) return 0;
        if (distance >= nearDist) return 1;
        return 2;
    }

    size_t polygonCount() const { return map_.size(); }
    void clear() { map_.clear(); lod_map_.clear(); fb_w_ = fb_h_ = 0; }

private:
    using LodSpans = std::vector<PixelSpan>[LOD_COUNT];
    std::unordered_map<PolygonID, std::vector<PixelSpan>> map_;
    std::unordered_map<PolygonID, LodSpans> lod_map_;
    int fb_w_ = 0, fb_h_ = 0;
    static const std::vector<PixelSpan> kEmpty;
};

inline const std::vector<PixelSpan> PixelMap::kEmpty{};

} // namespace dna
} // namespace mgd
