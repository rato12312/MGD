#pragma once

// Chaveiro: slots de chave AES-128 setados pelo USUÁRIO (nunca embarcadas).
// Descriptografa seção com CTR 128-bit explícito (sem adivinhar layout NCA).
// Suporta: header key (XTS), section keys (CTR), title keys.
// NEON acceleration para ARM64 (AES/SHA hardware acceleration).

#include <cstdint>
#include <cstring>
#include <vector>

#include "Aes.h"
#include "AesNeon.h"

namespace mgd {
namespace emu {

class KeyManager {
public:
    static constexpr int SLOTS = 16;

    KeyManager() {
        for (int i = 0; i < SLOTS; i++) have_[i] = false;
    }

    bool setSlot(int i, const uint8_t key[16]) {
        if (i < 0 || i >= SLOTS) return false;
        std::memcpy(keys_[i], key, 16);
        have_[i] = true;
        return true;
    }
    bool hasSlot(int i) const { return i >= 0 && i < SLOTS && have_[i]; }

    // CTR section crypto (simétrico)
    bool cryptSection(const uint8_t* in, uint8_t* out, size_t len, int slot,
                      const uint8_t ctr[16]) const {
        if (!hasSlot(slot)) return false;
        
#if MGD_HAS_NEON_CRYPTO
        ensureNeonKey(slot);
        // Use NEON-accelerated AES CTR
        aes::cryptCtrNeon(neok_[slot], ctr, in, out, len);
#else
        aes::cryptCtrFull(keys_[slot], ctr, in, out, len);
#endif
        return true;
    }

    // XTS header decrypt (primeiros 0xC00 bytes do NCA)
    bool xtsDecryptHeader(const uint8_t* in, uint8_t* out, int key_slot, const uint8_t tweak[16]) const {
        if (!hasSlot(key_slot)) return false;
        // NCA header size = 0xC00
        return aes::xtsDecrypt(keys_[key_slot], keys_[key_slot ^ 1], tweak, in, out, 0xC00);
    }

    // Decrypt NCA section: header -> section table -> sections
    // Usuario deve fornecer: header_key_slot (XTS), section_key_slots[4] (CTR)
    struct NcaDecryptResult {
        bool valid = false;
        std::vector<uint8_t> header; // 0xC00 bytes decrypted
        struct SectionInfo {
            uint64_t offset = 0;
            uint64_t size = 0;
            uint32_t key_slot = 0; // which slot was used
            std::vector<uint8_t> data; // decrypted
        };
        SectionInfo sections[4];
    };

    // Tenta descriptografar NCA completo. Retorna true se header OK.
    bool decryptNca(const uint8_t* nca, size_t nca_size, int header_key_slot,
                    const int section_key_slots[4], NcaDecryptResult& out) const {
        if (!hasSlot(header_key_slot)) return false;
        if (nca_size < 0xC00) return false;

        // 1. Decrypt header (XTS, tweak = sector 0)
        uint8_t tweak[16] = {0}; // sector 0
        out.header.resize(0xC00);
        if (!xtsDecryptHeader(nca, out.header.data(), header_key_slot, tweak)) return false;

        // 2. Parse FsHeader (at 0x200 in header)
        const uint8_t* hdr = out.header.data();
        if (std::memcmp(hdr, "NCA3", 4) != 0) return false;

        // FsHeader at 0x200: 4 sections, each 0x10 bytes (offset, size, key_index)
        const uint8_t* fs_hdr = hdr + 0x200;
        for (int i = 0; i < 4; i++) {
            uint64_t offset = rd64(fs_hdr + i * 0x10 + 0);
            uint64_t size = rd64(fs_hdr + i * 0x10 + 8);
            out.sections[i].offset = offset;
            out.sections[i].size = size;
            out.sections[i].key_slot = section_key_slots[i];
        }

        // 3. Decrypt each section (CTR with counter = offset / 0x200)
        for (int i = 0; i < 4; i++) {
            auto& sec = out.sections[i];
            if (sec.size == 0 || sec.key_slot == 0xFFFFFFFF) continue;
            if (!hasSlot(sec.key_slot)) continue;
            if (sec.offset + sec.size > nca_size) continue;

            uint8_t ctr[16] = {0};
            uint64_t ctr_val = sec.offset / 0x200; // sector number
            for (int b = 0; b < 8; b++) ctr[b] = static_cast<uint8_t>(ctr_val >> (8 * b));
            sec.data.resize(static_cast<size_t>(sec.size));
            if (!cryptSection(nca + sec.offset, sec.data.data(), static_cast<size_t>(sec.size), sec.key_slot, ctr)) {
                return false;
            }
        }
        out.valid = true;
        return true;
    }

private:
    static uint64_t rd64(const uint8_t* v, size_t o) {
        uint64_t r = 0;
        for (int i = 0; i < 8; i++) r |= static_cast<uint64_t>(v[o + i]) << (8 * i);
        return r;
    }

    uint8_t keys_[SLOTS][16] = {};
    bool have_[SLOTS] = {};

    // NEON-accelerated key expansion (cached per slot)
    mutable uint8x16_t neok_[SLOTS][11];
    mutable bool neok_valid_[SLOTS] = {};

    void ensureNeonKey(int slot) const {
#if MGD_HAS_NEON_CRYPTO
        if (!neok_valid_[slot]) {
            aes::expandKeyNeon(keys_[slot], const_cast<uint8x16_t*>(neok_[slot]));
            neok_valid_[slot] = true;
        }
#endif
    }
};

} // namespace emu
} // namespace mgd
