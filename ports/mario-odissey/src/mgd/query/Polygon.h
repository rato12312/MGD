#pragma once

#include "../common/Vec3.h"
#include "../common/Types.h"
#include <cstdint>

namespace mgd {

// Polygon — unidade consultável do sistema.
// Reutiliza Vec3 (position), PolygonID/AssetID (Types.h) e flags bitfield (BitSpace).
// Não copia asset inteiro, apenas referencia via AssetID.
// Mantém posição + identidade + referência, sem remover IDs.

struct Polygon {
    Vec3 position{};
    PolygonID polygon_id = INVALID_POLYGON_ID;
    AssetID asset_id = INVALID_ASSET_ID;
    uint32_t flags = 0; // BitSpace: visibilidade/estado/distância futura

    // helpers BitSpace — acesso rápido, sem packing indiscriminado
    bool hasFlag(uint32_t bit) const { return (flags & bit) != 0; }
    void setFlag(uint32_t bit) { flags |= bit; }
    void clearFlag(uint32_t bit) { flags &= ~bit; }

    // para classificação futura por distância/área projetada
    float distanceTo(const Vec3& p) const { return position.distanceTo(p); }
    float distanceSqTo(const Vec3& p) const { return position.distanceSqTo(p); }
};

// BitSpace — flags de polígono (usar bits só quando traz vantagem)
// 0 = polígono, 1 = linha (aresta que conecta 2 polígonos). Cada asset tem seus IDs próprios.
// Baixo consumo, boa localidade, acesso rápido.
namespace PolygonFlag {
    constexpr uint32_t TYPE_POLYGON = 0u << 0; // 0 = polígono (default)
    constexpr uint32_t TYPE_LINE    = 1u << 0; // 1 = linha/aresta entre 2 polígonos
    constexpr uint32_t TYPE_MASK    = 1u << 0;
    constexpr uint32_t VISIBLE      = 1u << 1;
    constexpr uint32_t OCCLUDED     = 1u << 2;
    constexpr uint32_t DIRTY        = 1u << 3;
    constexpr uint32_t STATIC       = 1u << 4;
    constexpr uint32_t TRANSPARENT  = 1u << 5;
    constexpr uint32_t COLLIDABLE   = 1u << 6;
    // reservados para LOD/distância futura: bits 16..23
    constexpr uint32_t LOD_MASK     = 0xFFu << 16;
}

} // namespace mgd
