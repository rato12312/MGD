#pragma once

// Mundo Odyssey no Mapa Mental: bootstrap do reino inicial + modo barato.
// Foco só no Mario Odyssey. Sem copiar assets: só IDs + posição.
// Reaproveita core/bridge (MentalMapRuntime) e core/query (DNA, polígonos).

#include <cstdint>
#include <cstdio>
#include <vector>

#include "core/bridge/MentalMapRuntime.h"

namespace mgd {
namespace odyssey {

// Modo barato (espelha ports/mario-odissey/odyssey-edge.ini).
// Mali só entrega rascunho; o painter completa.
struct CheapMode {
    float resolution_factor = 0.5f;
    bool shadows = false;
    bool anti_aliasing = false;
    bool post_processing = false;
    bool frame_reuse_static = true;
    bool lod_aggressive = true;

    static CheapMode edge() {
        CheapMode c;
        c.resolution_factor = 0.4f;
        return c;
    }
};

// Reino inicial (Cap Kingdom): fileira de polígonos sintéticos para boot.
// Quando a captura real existir, o handoff substitui esta fonte.
class OdysseyWorld {
public:
    OdysseyWorld() : rt_(64, 32) {}

    void cheap(const CheapMode& c) { cheap_ = c; }
    const CheapMode& cheap() const { return cheap_; }

    // Boot: alimenta o mapa mental com N polígonos do reino. Retorna stats.
    // Streaming: teto de RAM no cache (barato = teto menor, região velha cai).
    bridge::RuntimeFrameStats boot(uint32_t n = 20) {
        rt_.cache().setBudget(cheap_.resolution_factor <= 0.4f ? 1024 : 4096);
        bridge::HandoffFrame f;
        f.frame_index = 1;
        f.camera.position = Vec3(0.0f, 8.0f, 20.0f);
        f.camera.forward = Vec3(0.0f, 0.0f, -1.0f);
        std::vector<Polygon> polys = makeKingdom(n);
        for (const auto& p : polys) f.visible_polygons.push_back(p.polygon_id);
        return rt_.step(f, polys);
    }

    // Frame parado: mesmo handoff, nada muda -> painter reaproveita.
    bridge::RuntimeFrameStats idleFrame(uint64_t index, uint32_t n = 20) {
        bridge::HandoffFrame f;
        f.frame_index = index;
        f.camera.position = Vec3(0.0f, 8.0f, 20.0f);
        std::vector<Polygon> polys = makeKingdom(n);
        for (const auto& p : polys) f.visible_polygons.push_back(p.polygon_id);
        return rt_.step(f, polys);
    }

    bridge::MentalMapRuntime& runtime() { return rt_; }

    // Painter apresenta: despeja o framebuffer do pipeline em PPM (P6).
    bool present(const char* path) {
        const Framebuffer& fb = rt_.pipeline().framebuffer();
        int w = fb.width(), h = fb.height();
        if (w <= 0 || h <= 0) return false;
        FILE* f = std::fopen(path, "wb");
        if (!f) return false;
        std::fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                RGBA c = fb.getPixel(x, y);
                uint8_t rgb[3] = {c.r, c.g, c.b};
                if (std::fwrite(rgb, 1, 3, f) != 3) { std::fclose(f); return false; }
            }
        }
        std::fclose(f);
        return true;
    }

private:
    static std::vector<Polygon> makeKingdom(uint32_t n) {
        std::vector<Polygon> polys;
        for (uint32_t i = 1; i <= n; ++i) {
            Polygon p;
            p.position = Vec3(static_cast<float>(i), 0.0f, 0.0f);
            p.polygon_id = 9000 + i; // faixa do Odyssey (não colide com testes)
            p.asset_id = 42;
            p.flags = PolygonFlag::VISIBLE;
            polys.push_back(p);
        }
        return polys;
    }

    bridge::MentalMapRuntime rt_;
    CheapMode cheap_;
};

} // namespace odyssey
} // namespace mgd
