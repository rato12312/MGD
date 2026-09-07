#pragma once

// Loader NSO: magic "NSO0", 3 segmentos (LZ4 se flag ligada) + .bss.
// Layout pela switchbrew (conferir contra a wiki se algo falhar):
// 0x00 magic, 0x0C flags(bit0=text,bit1=ro,bit2=data comprimidos),
// 0x10/0x20/0x30 segmentos (file_off, mem_off, decomp_size),
// 0x38 bss_size, 0x60/0x64/0x68 tamanhos comprimidos.

#include <cstdint>
#include <cstring>
#include <vector>

#include "Lz4.h"

namespace mgd {
namespace emu {

struct NsoSegment {
    uint32_t file_offset = 0;
    uint32_t memory_offset = 0;
    uint32_t decomp_size = 0;
    uint32_t comp_size = 0;
};

struct NsoImage {
    bool valid = false;
    bool comp_text = false, comp_ro = false, comp_data = false;
    NsoSegment text, ro, data;
    uint32_t bss_size = 0;
};

inline NsoImage parseNso(const uint8_t* blob, size_t len) {
    NsoImage img;
    if (len < 0x6C) return img;
    uint32_t magic = 0;
    std::memcpy(&magic, blob, 4);
    if (magic != 0x304F534E) return img; // "NSO0"
    auto rd32 = [&](size_t off) {
        uint32_t v = 0;
        std::memcpy(&v, blob + off, 4);
        return v;
    };
    uint32_t flags = rd32(0x0C);
    img.comp_text = (flags & 1) != 0;
    img.comp_ro = (flags & 2) != 0;
    img.comp_data = (flags & 4) != 0;
    auto seg = [&](size_t off) {
        NsoSegment s;
        s.file_offset = rd32(off);
        s.memory_offset = rd32(off + 4);
        s.decomp_size = rd32(off + 8);
        return s;
    };
    img.text = seg(0x10);
    img.ro = seg(0x20);
    img.data = seg(0x30);
    img.bss_size = rd32(0x38);
    img.text.comp_size = rd32(0x60);
    img.ro.comp_size = rd32(0x64);
    img.data.comp_size = rd32(0x68);
    const NsoSegment* segs[3] = {&img.text, &img.ro, &img.data};
    for (int i = 0; i < 3; i++) {
        uint64_t need = segs[i]->file_offset + (i == 0 ? (img.comp_text ? img.text.comp_size : img.text.decomp_size)
                                        : i == 1 ? (img.comp_ro ? img.ro.comp_size : img.ro.decomp_size)
                                                 : (img.comp_data ? img.data.comp_size : img.data.decomp_size));
        if (need > len) return NsoImage{};
    }
    img.valid = true;
    return img;
}

// Descomprime (se preciso) e mapeia em mem[base..]. Entry = início do .text.
inline bool loadNsoInto(const NsoImage& img, const uint8_t* blob, uint8_t* mem,
                        uint64_t memSize, uint64_t base, uint64_t& entryOut) {
    if (!img.valid) return false;
    const NsoSegment* segs[3] = {&img.text, &img.ro, &img.data};
    const bool comp[3] = {img.comp_text, img.comp_ro, img.comp_data};
    for (int i = 0; i < 3; i++) {
        uint64_t dst = base + segs[i]->memory_offset;
        if (dst + segs[i]->decomp_size > memSize) return false;
        if (comp[i]) {
            std::vector<uint8_t> out;
            if (!lz4::decompressBlock(blob + segs[i]->file_offset, segs[i]->comp_size, out))
                return false;
            if (out.size() != segs[i]->decomp_size) return false;
            std::memcpy(mem + dst, out.data(), out.size());
        } else {
            std::memcpy(mem + dst, blob + segs[i]->file_offset, segs[i]->decomp_size);
        }
    }
    entryOut = base + img.text.memory_offset;
    return true;
}

} // namespace emu
} // namespace mgd
