#pragma once

// LZ4 block (raw, sem frame): literais + matches.
// Implementação mínima para descomprimir segmentos NSO.
// Formato: token [ext-lit] literais offset(LE16) [ext-match] ... fim em literais.

// Decomprime src[0..srcLen) em out. false = bloco inválido/truncado.

#include <cstdint>
#include <vector>

namespace mgd {
namespace emu {
namespace lz4 {

inline bool decompressBlock(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out) {
    out.clear();
    size_t p = 0;
    while (true) {
        if (p >= srcLen) return false;
        uint8_t token = src[p++];
        size_t litLen = token >> 4;
        if (litLen == 15) {
            uint8_t b = 0;
            do {
                if (p >= srcLen) return false;
                b = src[p++];
                litLen += b;
            } while (b == 255);
        }
        if (p + litLen > srcLen) return false;
        out.insert(out.end(), src + p, src + p + litLen);
        p += litLen;
        if (p >= srcLen) break; // termina em literais (fim do bloco)
        if (p + 2 > srcLen) return false;
        size_t offset = static_cast<size_t>(src[p]) | (static_cast<size_t>(src[p + 1]) << 8);
        p += 2;
        if (offset == 0 || offset > out.size()) return false;
        size_t matchLen = (token & 0xF) + 4;
        if ((token & 0xF) == 15) {
            uint8_t b = 0;
            do {
                if (p >= srcLen) return false;
                b = src[p++];
                matchLen += b;
            } while (b == 255);
        }
        size_t from = out.size() - offset;
        for (size_t i = 0; i < matchLen; i++) out.push_back(out[from + i]);
    }
    return true;
}

} // namespace lz4
} // namespace emu
} // namespace mgd
