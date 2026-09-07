#pragma once

// Sonda NCA: só identifica (magic + tipo). O resto (seções, cripto)
// entra com a wiki aberta — sem adivinhar offsets aqui.
// Header tem 0x400 bytes; magic "NCA3"/"NCA2" em 0x200.

#include <cstdint>
#include <cstring>

namespace mgd {
namespace emu {

struct NcaProbe {
    bool valid = false;
    char magic[5] = {0, 0, 0, 0, 0};
    uint8_t distribution = 0;  // 0x204 (a conferir)
    uint8_t content_type = 0;  // 0x205 (0=programa,1=meta,2=controle,3=manual,4=dados)
};

inline NcaProbe probeNca(const uint8_t* blob, size_t len) {
    NcaProbe p;
    if (len < 0x400) return p;
    if (std::memcmp(blob + 0x200, "NCA", 3) != 0) return p;
    char v = static_cast<char>(blob[0x203]);
    if (v != '0' && v != '2' && v != '3') return p;
    std::memcpy(p.magic, blob + 0x200, 4);
    p.distribution = blob[0x204];
    p.content_type = blob[0x205];
    p.valid = true;
    return p;
}

} // namespace emu
} // namespace mgd
