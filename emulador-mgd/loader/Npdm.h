#pragma once

// NPDM (metadados do programa): META + ACID + ACI (switchbrew).
// Offsets fac/sac/kc lidos relativos ao início do ACID (convenção aqui).
// Extrai: 64-bit, stack, nome, programId, serviços, ThreadInfo.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mgd {
namespace emu {

struct NpdmInfo {
    bool valid = false;
    bool is64 = false;
    uint32_t addr_space = 0;
    uint8_t main_prio = 0;
    uint32_t main_stack = 0;
    std::string name;
    uint64_t program_id = 0;
    std::vector<std::string> services;
    bool has_thread_info = false;
    uint32_t thread_low = 0, thread_high = 0;
    uint32_t min_core = 0, max_core = 0;
};

inline NpdmInfo parseNpdm(const uint8_t* blob, size_t len) {
    NpdmInfo out;
    auto rd32 = [&](size_t off) {
        uint32_t v = 0;
        if (off + 4 <= len) std::memcpy(&v, blob + off, 4);
        return v;
    };
    auto rd64 = [&](size_t off) {
        uint64_t v = 0;
        if (off + 8 <= len) std::memcpy(&v, blob + off, 8);
        return v;
    };
    if (len < 0x80) return out;
    if (std::memcmp(blob, "META", 4) != 0) return out;
    uint8_t flags = blob[0x0C];
    out.is64 = (flags & 1) != 0;
    out.addr_space = (flags >> 1) & 7;
    out.main_prio = blob[0x0E];
    out.main_stack = rd32(0x1C);
    char nm[17] = {0};
    if (0x20 + 16 <= len) std::memcpy(nm, blob + 0x20, 16);
    out.name = nm;
    uint32_t aci_off = rd32(0x70), aci_size = rd32(0x74);
    uint32_t acid_off = rd32(0x78), acid_size = rd32(0x7C);
    if (acid_off + acid_size > len || aci_off + aci_size > len) return out;
    // ACI: magic + programId
    if (std::memcmp(blob + aci_off, "ACI0", 4) != 0) return out;
    out.program_id = rd64(aci_off + 0x10);
    // ACID: magic + sac + kc
    if (std::memcmp(blob + acid_off + 0x200, "ACID", 4) != 0) return out;
    uint32_t sac_off = rd32(acid_off + 0x228);
    uint32_t sac_size = rd32(acid_off + 0x22C);
    uint32_t kc_off = rd32(acid_off + 0x230);
    uint32_t kc_size = rd32(acid_off + 0x234);
    size_t sp = static_cast<size_t>(acid_off) + sac_off;
    size_t se = sp + sac_size;
    int guard = 0;
    while (sp < se && sp < len && guard < 64) {
        guard++;
        uint8_t b = blob[sp];
        size_t nlen = static_cast<size_t>(b & 7) + 1;
        if (sp + 1 + nlen > len || sp + 1 + nlen > se) break;
        out.services.push_back(std::string(reinterpret_cast<const char*>(blob + sp + 1), nlen));
        sp += 1 + nlen;
    }
    size_t kp = static_cast<size_t>(acid_off) + kc_off;
    size_t ke = kp + kc_size;
    guard = 0;
    while (kp + 4 <= ke && kp + 4 <= len && guard < 64) {
        guard++;
        uint32_t d = 0;
        std::memcpy(&d, blob + kp, 4);
        if ((d & 0xF) == 0x7) { // ThreadInfo: lowest clear = bit3
            out.has_thread_info = true;
            out.thread_low = (d >> 4) & 0x3F;
            out.thread_high = (d >> 10) & 0x3F;
            out.min_core = (d >> 16) & 0xFF;
            out.max_core = (d >> 24) & 0xFF;
        }
        kp += 4;
    }
    out.valid = true;
    return out;
}

} // namespace emu
} // namespace mgd
