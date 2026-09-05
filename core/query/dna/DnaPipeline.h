#pragma once

#include "Dna.h"
#include "XyzIndex.h"
#include "PixelMap.h"
#include "PixelCache.h"
#include "ChangeDetector.h"
#include "../Polygon.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mgd {
namespace dna {

// Junta tudo: JOGO -> DNA/Índices -> Mapa Mental -> Mapa de Pixels ->
// Código de Cor -> Pixel Cache -> Framebuffer -> Tela.
struct DnaFrameStats {
    uint32_t rebuilt_polys = 0;
    uint32_t reused_polys = 0;
    uint32_t pixels_written = 0;
    uint32_t pixels_dirty = 0;
};

class DnaPipeline {
public:
    DnaPipeline() = default;
    DnaPipeline(int fbW, int fbH) : cache_(fbW, fbH) { fb_.resize(fbW, fbH); }

    void resize(int fbW, int fbH) {
        cache_.resize(fbW, fbH);
        fb_.resize(fbW, fbH);
    }

    // Constrói DNA + índices a partir dos polígonos (uma vez / quando muda).
    void buildFromPolygons(const std::vector<Polygon>& polys, uint8_t colorBase = 10) {
        dnas_.clear();
        dnas_.reserve(polys.size());
        for (const auto& p : polys) {
            DnaPolygon d;
            d.polygon_id = p.polygon_id;
            d.asset_id = p.asset_id;
            d.xyz_id = xyz_.intern(p.position);
            d.geo_code = static_cast<uint16_t>(p.polygon_id % 8u);
            d.color_code = ColorCode::encode(colorBase, static_cast<int>(p.polygon_id % 5) - 2);
            d.flags = p.flags;
            dnas_.push_back(d);
            detector_.touchPolygon(d.polygon_id); // registra versão inicial
            last_poly_ver_[d.polygon_id] = 0; // força cálculo no primeiro frame
        }
        pixel_map_.buildStatic(dnas_, xyz_, fb_.width(), fb_.height());
    }

    // Render de um frame com detecção de mudanças + cache incremental.
    // changeMask: polygon_ids que mudaram neste frame (mundo/objeto já tratado fora).
    DnaFrameStats renderFrame(const std::vector<PolygonID>& changedPolys) {
        // Sem câmera: LOD médio para todos (comportamento anterior preservado).
        return renderFrameLod(changedPolys, Vec3{}, false);
    }

    // Com câmera: a distância seleciona a representação preparada (LOD 0/1/2),
    // sem reconstruir nada — perto usa detalhe, longe usa compacto.
    DnaFrameStats renderFrameLod(const std::vector<PolygonID>& changedPolys,
                                 const Vec3& cameraPos, bool useLod,
                                 float nearDist = 20.0f, float farDist = 60.0f) {
        DnaFrameStats stats;
        for (PolygonID pid : changedPolys) detector_.touchPolygon(pid);
        for (const auto& d : dnas_) {
            bool changed = detector_.polygonChanged(d.polygon_id, last_world_ver_, last_poly_ver_[d.polygon_id]);
            if (!changed) { stats.reused_polys++; continue; }
            stats.rebuilt_polys++;
            last_poly_ver_[d.polygon_id] = detector_.polygonVersion(d.polygon_id);
            if (useLod) {
                Vec3 p = xyz_.decode(d.xyz_id);
                float dist = p.distanceTo(cameraPos);
                int lod = PixelMap::selectLod(dist, nearDist, farDist);
                for (const auto& span : pixel_map_.getLod(d.polygon_id, lod)) {
                    cache_.setPixelCode(span.pixel_index, span.color_code);
                }
            } else {
                for (const auto& span : pixel_map_.get(d.polygon_id)) {
                    cache_.setPixelCode(span.pixel_index, span.color_code);
                }
            }
        }
        last_world_ver_ = detector_.worldVersion();
        stats.pixels_dirty = cache_.dirtyCount();
        stats.pixels_written = cache_.flush(fb_);
        return stats;
    }

    const Framebuffer& framebuffer() const { return fb_; }
    const XyzIndex& xyz() const { return xyz_; }
    const std::vector<DnaPolygon>& dnas() const { return dnas_; }
    ChangeDetector& detector() { return detector_; }

private:
    XyzIndex xyz_;
    PixelMap pixel_map_;
    IncrementalPixelCache cache_;
    ChangeDetector detector_;
    Framebuffer fb_;
    std::vector<DnaPolygon> dnas_;
    std::unordered_map<PolygonID, uint64_t> last_poly_ver_;
    uint64_t last_world_ver_ = 1;
};

} // namespace dna
} // namespace mgd
