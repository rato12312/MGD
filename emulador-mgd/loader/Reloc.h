#pragma once

// Relocações ELF64 R_AARCH64_RELATIVE (tipo 1027): *reloc = base + addend.
// Sem isso NRO/NSO real não roda (endereços absolutos). Formato ELF puro,
// sem chute: tabela de Elf64_Rela explícita (a descoberta via MOD0/DYNAMIC
// entra quando confirmada contra a wiki).

#include <cstdint>
#include <cstring>
#include <vector>

namespace mgd {
namespace emu {

struct Elf64Rela {
    uint64_t offset = 0;
    uint64_t info = 0;
    int64_t addend = 0;
};

inline bool applyRelativeRelocs(uint8_t* image, uint64_t imageSize,
                                const Elf64Rela* relas, size_t count,
                                uint64_t base, uint64_t* appliedOut = nullptr) {
    uint64_t applied = 0;
    for (size_t i = 0; i < count; i++) {
        uint32_t type = static_cast<uint32_t>(relas[i].info & 0xFFFFFFFFull);
        if (type != 1027) return false; // só RELATIVE aqui; resto é outro passo
        uint64_t dst = base + relas[i].offset;
        if (dst + 8 > base + imageSize) return false;
        // dst é VA; image[0] == base (imagem mapeada na base)
        uint64_t off = dst - base;
        uint64_t v = base + static_cast<uint64_t>(relas[i].addend);
        std::memcpy(image + off, &v, 8);
        applied++;
    }
    if (appliedOut) *appliedOut = applied;
    return true;
}

} // namespace emu
} // namespace mgd
