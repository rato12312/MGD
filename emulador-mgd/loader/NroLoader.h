#pragma once

// Loader NRO: lê cabeçalho (magic "NRO0" em 0x10), deposita os segmentos
// contíguos (.text em 0x80, depois .rodata, depois .data) na RAM e devolve
// o entry point. Formato: https://switchbrew.org/wiki/NRO

#include <cstdint>
#include <cstring>

namespace mgd {
namespace emu {

struct NroSegment {
    uint32_t memory_offset = 0; // destino na imagem (relativo à base)
    uint32_t size = 0;
    uint32_t file_offset = 0; // calculado: contíguo a partir de 0x80
};

struct NroImage {
    bool valid = false;
    NroSegment text, rodata, data;
    uint32_t bss_size = 0;
};

inline NroImage parseNro(const uint8_t* blob, size_t len) {
    NroImage img;
    if (len < 0x80) return img;
    uint32_t magic = 0;
    std::memcpy(&magic, blob + 0x10, 4);
    if (magic != 0x304F524E) return img; // "NRO0" little-endian
    auto rd32 = [&](size_t off) {
        uint32_t v = 0;
        std::memcpy(&v, blob + off, 4);
        return v;
    };
    img.text.memory_offset = rd32(0x20);
    img.text.size = rd32(0x24);
    img.rodata.memory_offset = rd32(0x30);
    img.rodata.size = rd32(0x34);
    img.data.memory_offset = rd32(0x40);
    img.data.size = rd32(0x44);
    img.bss_size = rd32(0x48);
    // arquivo: segmentos contíguos a partir de 0x80
    img.text.file_offset = 0x80;
    img.rodata.file_offset = 0x80 + img.text.size;
    img.data.file_offset = 0x80 + img.text.size + img.rodata.size;
    const NroSegment* segs[3] = {&img.text, &img.rodata, &img.data};
    for (int i = 0; i < 3; i++) {
        if (static_cast<uint64_t>(segs[i]->file_offset) + segs[i]->size > len) return NroImage{};
    }
    img.valid = true;
    return img;
}

// Mapeia a imagem em mem[base..] (header junto). Entry = base (Start).
inline bool loadNroInto(const NroImage& img, const uint8_t* blob, uint8_t* mem,
                        uint64_t memSize, uint64_t base, uint64_t& entryOut) {
    if (!img.valid) return false;
    if (base + 0x80 > memSize) return false;
    std::memcpy(mem + base, blob, 0x80); // header junto (NRO roda mapeado)
    const NroSegment* segs[3] = {&img.text, &img.rodata, &img.data};
    for (int i = 0; i < 3; i++) {
        uint64_t dst = base + segs[i]->memory_offset;
        if (dst + segs[i]->size > memSize) return false;
        std::memcpy(mem + dst, blob + segs[i]->file_offset, segs[i]->size);
    }
    entryOut = base;
    return true;
}

} // namespace emu
} // namespace mgd
