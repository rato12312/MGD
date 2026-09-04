#pragma once

#include "../../common/RGBA.h"
#include <array>
#include <cstdint>

namespace mgd {
namespace dna {

// Sistema de cores por código: cor-base + variação via lookup table.
// Ex.: base 10 (amarelo), +1 mais claro, -1 mais escuro — sem recalcular por pixel.
class ColorCode {
public:
    // base: 0..31, variation: -3..+3 mapeado para 0..6 (+3 offset)
    static constexpr uint16_t encode(uint8_t base, int variation) {
        if (base > 31) base = 31;
        int v = variation + 3;
        if (v < 0) v = 0;
        if (v > 6) v = 6;
        return static_cast<uint16_t>(base * 8u + static_cast<uint32_t>(v));
    }
    static constexpr uint8_t baseOf(uint16_t code) { return static_cast<uint8_t>(code / 8u); }
    static constexpr int variationOf(uint16_t code) { return static_cast<int>(code % 8u) - 3; }

    // Paleta determinística: 32 bases em gradiente + variação como deslocamento.
    // LUT evita recalcular a transformação para cada pixel.
    static RGBA decode(uint16_t code) {
        uint8_t base = baseOf(code);
        int var = variationOf(code);
        // base -> matiz simples e estável (sem dependências externas)
        uint8_t r = static_cast<uint8_t>((base * 67u + 40u) % 256u);
        uint8_t g = static_cast<uint8_t>((base * 131u + 90u) % 256u);
        uint8_t b = static_cast<uint8_t>((base * 197u + 140u) % 256u);
        int shift = var * 18;
        auto adj = [shift](uint8_t c) -> uint8_t {
            int v = static_cast<int>(c) + shift;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            return static_cast<uint8_t>(v);
        };
        return RGBA(adj(r), adj(g), adj(b), 255);
    }
};

} // namespace dna
} // namespace mgd
