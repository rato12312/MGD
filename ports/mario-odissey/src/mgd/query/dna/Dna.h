#pragma once

#include "../../common/Types.h"
#include <cstdint>

namespace mgd {
namespace dna {

// DNA de polígono: representação compacta (identificação + posição via índice +
// geometria + referências + visual). Evita reconstruir/procurar tudo de novo.
struct DnaPolygon {
    PolygonID polygon_id = INVALID_POLYGON_ID;
    AssetID asset_id = INVALID_ASSET_ID;
    uint32_t xyz_id = 0;      // índice em XyzIndex (Código -> X/Y/Z)
    uint16_t geo_code = 0;    // classe geométrica (tri retângulo, faixa, etc.)
    uint16_t color_code = 0;  // cor-base + variação (ColorCode)
    uint32_t flags = 0;       // BitSpace reutilizado (PolygonFlag)
};

} // namespace dna
} // namespace mgd
