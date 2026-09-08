#pragma once

// Seções NCA (switchbrew): FsEntry[4] em 0x240 (start/end em blocos 0x200),
// FsHeader em +0x400+(id*0x200): fstype (0=RomFS,1=PFS) + crypto
// (0=Auto,1=None,2=XTS,3=CTR,4=CtrEx).
// Trabalha no header EM CLARO (dump real vem XTS nos 0xC00 primeiros;
// a chave do header é do usuário; XTS entra quando preciso).

#include <cstdint>
#include <cstring>
#include <vector>

namespace mgd {
namespace emu {

struct NcaSection {
    bool used = false;
    uint64_t data_offset = 0; // bytes, do início do NCA
    uint64_t data_size = 0;
    uint8_t fs_type = 0;    // 0=RomFS 1=PartitionFS
    uint8_t crypto = 0;     // 0=Auto 1=None 2=XTS 3=CTR 4=CtrEx
};

struct NcaLayout {
    bool valid = false;
    NcaSection sections[4];
};

inline NcaLayout parseNcaSections(const uint8_t* hdr, size_t len) {
    NcaLayout out;
    if (len < 0x400 + 4 * 0x200) return out;
    auto rd32 = [&](size_t off) {
        uint32_t v = 0;
        std::memcpy(&v, hdr + off, 4);
        return v;
    };
    for (int i = 0; i < 4; i++) {
        uint32_t start = rd32(0x240 + i * 0x10);
        uint32_t end = rd32(0x240 + i * 0x10 + 4);
        if (end <= start) continue;
        size_t fsh = static_cast<size_t>(0x400) + static_cast<size_t>(i) * 0x200;
        if ((rd32(fsh) & 0xFFFFu) != 2) continue; // version u16 tem que ser 2
        NcaSection& s = out.sections[i];
        s.used = true;
        s.data_offset = static_cast<uint64_t>(start) * 0x200ull;
        s.data_size = static_cast<uint64_t>(end - start) * 0x200ull;
        s.fs_type = hdr[fsh + 2];
        s.crypto = hdr[fsh + 4];
    }
    out.valid = true;
    return out;
}

} // namespace emu
} // namespace mgd
