#pragma once

// Chaves MGD: o usuário decide onde o MGD assume.
// - translation: RAM passa pela MMU (mapa + permissão) ou direto.
// - mali: Full (sem tocar) / Cheap (0.5x) / Edge (0.4x agressivo).
// - image: painter apresenta/completa o frame ou não.

#include "../odyssey/OdysseyWorld.h"

namespace mgd {
namespace emu {

enum class MaliLevel { Full = 0, Cheap = 1, Edge = 2 };

struct MgdSwitches {
    bool mgd_translation = true;
    MaliLevel mgd_mali = MaliLevel::Cheap;
    bool mgd_image = true;

    odyssey::CheapMode cheapForLevel() const {
        odyssey::CheapMode c;
        if (mgd_mali == MaliLevel::Full) {
            c.resolution_factor = 1.0f;
            c.shadows = true;
            c.anti_aliasing = true;
            c.post_processing = true;
            c.frame_reuse_static = false;
            c.lod_aggressive = false;
        } else if (mgd_mali == MaliLevel::Edge) {
            return odyssey::CheapMode::edge();
        }
        return c;
    }
};

} // namespace emu
} // namespace mgd
