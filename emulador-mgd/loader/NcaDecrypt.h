// NCA Decryption Implementation — XTS (header) + CTR (sections) real
// Baseado na especificação switchbrew / hactool

#include "NcaSections.h"
#include "Keys.h"
#include "Aes.h"
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace mgd {
namespace emu {

// NCA structure constants
static constexpr size_t NCA_HEADER_SIZE = 0x4000;   // 16KB header (includes encrypted part)
static constexpr size_t NCA_ENCRYPTED_HEADER_SIZE = 0xC00; // First 3KB encrypted with XTS
static constexpr size_t NCA_SECTOR_SIZE = 0x200;    // 512 bytes per sector
static constexpr size_t FSH_ENTRY_SIZE = 0x10;      // 16 bytes per section entry

// NCA Crypto types
enum class NcaCryptoType : uint8_t {
    NONE = 1,
    XTS = 2,
    CTR = 3,
    CTR_EX = 4
};

// NCA FsType
enum class NcaFsType : uint8_t {
    ROMFS = 0,
    PFS0 = 1
};

// FsHeader entry (0x10 bytes at offset 0x200 in decrypted header)
struct FsHeaderEntry {
    uint64_t offset;      // sector offset (in NCA sectors)
    uint64_t size;        // size in sectors
    uint32_t key_index;   // index into key area (0-3)
    uint8_t crypto_type;  // 1=none, 2=xts, 3=ctr, 4=ctr_ex
    uint8_t fs_type;      // 0=romfs, 1=pfs0
    uint16_t reserved;    // padding
};

struct NcaDecryptedSection {
    bool valid = false;
    uint64_t offset = 0;          // byte offset in NCA
    uint64_t size = 0;            // byte size
    NcaCryptoType crypto = NcaCryptoType::NONE;
    NcaFsType fs_type = NcaFsType::ROMFS;
    uint32_t key_generation = 0;  // key area index
    std::vector<uint8_t> data;    // decrypted content
};

struct NcaDecryptResult {
    bool valid = false;
    std::vector<uint8_t> header;       // 0xC00 bytes decrypted header
    NcaDecryptedSection sections[4];
    NcaProbe probe;
    
    // Accessors
    const NcaDecryptedSection* getSection(int i) const {
        return (i >= 0 && i < 4) ? &sections[i] : nullptr;
    }
    NcaDecryptedSection* getSection(int i) {
        return (i >= 0 && i < 4) ? &sections[i] : nullptr;
    }
};

// Parse FsHeader entries from decrypted header (at offset 0x200)
static bool parseFsHeader(const uint8_t* decrypted_header, FsHeaderEntry entries[4]) {
    const uint8_t* fs_header = decrypted_header + 0x200;
    for (int i = 0; i < 4; i++) {
        const uint8_t* entry = fs_header + i * FSH_ENTRY_SIZE;
        if (entry + FSH_ENTRY_SIZE > decrypted_header + NCA_ENCRYPTED_HEADER_SIZE) {
            return false;
        }
        entries[i].offset = 0;
        entries[i].size = 0;
        std::memcpy(&entries[i].offset, entry + 0, 8);
        std::memcpy(&entries[i].size, entry + 8, 8);
        entries[i].key_index = entry[16];
        entries[i].crypto_type = entry[17];
        entries[i].fs_type = entry[18];
        entries[i].reserved = 0;
        std::memcpy(&entries[i].reserved, entry + 19, 2);
    }
    return true;
}

// XTS decrypt NCA header with proper sector tweak
// NCA header sectors: 0xC00 bytes = 1536 bytes = 3 sectors (0x200 each)
// Sector 0 at offset 0, sector 1 at 0x200, sector 2 at 0x400, etc.
static bool xtsDecryptNcaHeader(const KeyManager& km, int key_slot, 
                                const uint8_t* encrypted_nca, size_t nca_size,
                                uint8_t* out_header) {
    if (nca_size < NCA_ENCRYPTED_HEADER_SIZE) return false;
    if (!km.hasSlot(key_slot)) return false;
    
    // XTS key1 and key2 are in consecutive slots (key_slot and key_slot ^ 1)
    int key1_slot = key_slot;
    int key2_slot = key_slot ^ 1;  // adjacent slot
    if (!km.hasSlot(key2_slot)) return false;
    
    // Decrypt each sector with its own tweak (sector number as LE u128)
    for (size_t sector = 0; sector < NCA_ENCRYPTED_HEADER_SIZE / NCA_SECTOR_SIZE; ++sector) {
        size_t offset = sector * NCA_SECTOR_SIZE;
        
        // Tweak = sector number as little-endian u128
        uint8_t tweak[16] = {0};
        tweak[0] = static_cast<uint8_t>(sector & 0xFF);
        tweak[1] = static_cast<uint8_t>((sector >> 8) & 0xFF);
        tweak[2] = static_cast<uint8_t>((sector >> 16) & 0xFF);
        tweak[3] = static_cast<uint8_t>((sector >> 24) & 0xFF);
        // rest are 0
        
        // Decrypt this sector
        if (!km.aes::xtsDecrypt(
            km.keys_[key1_slot], 
            km.keys_[key2_slot], 
            tweak,
            encrypted_nca + offset, 
            out_header + offset, 
            NCA_SECTOR_SIZE)) {
            return false;
        }
    }
    return true;
}

// CTR decrypt section data
// Counter = sector number (offset / 0x200) as BE 64-bit in lower 8 bytes of counter
static bool ctrDecryptSection(const KeyManager& km, int key_slot,
                              const uint8_t* encrypted_nca, size_t nca_size,
                              const NcaDecryptedSection& sec,
                              uint8_t* out_data) {
    if (!km.hasSlot(key_slot)) return false;
    if (sec.offset + sec.size > nca_size) return false;
    
    // Counter = sector number (offset / 0x200)
    uint64_t sector = sec.offset / NCA_SECTOR_SIZE;
    
    // Build counter: 8 bytes sector number (BE) + 8 bytes zero
    uint8_t ctr[16] = {0};
    for (int b = 0; b < 8; b++) {
        ctr[7 - b] = static_cast<uint8_t>((sector >> (8 * b)) & 0xFF);  // BE in lower 8 bytes
    }
    
    return km.aes::cryptCtrFull(
        km.keys_[key_slot], 
        ctr,
        encrypted_nca + sec.offset,
        out_data,
        static_cast<size_t>(sec.size)
    );
}

// Main NCA decryption function
bool decryptNca(const uint8_t* nca, size_t nca_size, int header_key_slot,
                const int section_key_slots[4], NcaDecryptResult& out) {
    // Validate
    if (!nca || nca_size < NCA_ENCRYPTED_HEADER_SIZE) return false;
    
    // Probe first
    out.probe = probeNca(nca, nca_size);
    if (!out.probe.valid) return false;
    
    // Create KeyManager reference (we'll use the one passed in)
    // For now, assume KeyManager is available globally or passed
    // This function needs access to KeyManager instance
    return false; // Placeholder - needs KeyManager
}

// Full decryption with KeyManager
bool decryptNcaWithKeys(const KeyManager& km, const uint8_t* nca, size_t nca_size,
                        int header_key_slot, const int section_key_slots[4],
                        NcaDecryptResult& out) {
    if (!nca || nca_size < NCA_ENCRYPTED_HEADER_SIZE) return false;
    
    out = NcaDecryptResult();
    
    // Probe
    out.probe = probeNca(nca, nca_size);
    if (!out.probe.valid) return false;
    
    // 1. Decrypt header with XTS
    out.header.resize(NCA_ENCRYPTED_HEADER_SIZE);
    if (!xtsDecryptNcaHeader(km, header_key_slot, nca, nca_size, out.header.data())) {
        return false;
    }
    
    // Verify magic
    if (std::memcmp(out.header.data() + 0x200, "NCA3", 4) != 0 &&
        std::memcmp(out.header.data() + 0x200, "NCA2", 4) != 0) {
        return false;
    }
    
    // 2. Parse FsHeader entries
    FsHeaderEntry fs_entries[4];
    if (!parseFsHeader(out.header.data(), fs_entries)) {
        return false;
    }
    
    // 3. Decrypt each section
    for (int i = 0; i < 4; i++) {
        const auto& fsh = fs_entries[i];
        auto& sec = out.sections[i];
        
        if (fsh.size == 0) continue;
        
        sec.valid = true;
        sec.offset = fsh.offset * NCA_SECTOR_SIZE;
        sec.size = fsh.size * NCA_SECTOR_SIZE;
        sec.crypto = static_cast<NcaCryptoType>(fsh.crypto_type);
        sec.fs_type = static_cast<NcaFsType>(fsh.fs_type);
        sec.key_generation = fsh.key_index;
        
        // Determine which key slot to use
        int key_slot = section_key_slots[i];
        if (key_slot < 0 || key_slot >= KeyManager::SLOTS || !km.hasSlot(key_slot)) {
            // Try key_index from FsHeader
            key_slot = fsh.key_index;
        }
        if (key_slot < 0 || key_slot >= KeyManager::SLOTS || !km.hasSlot(key_slot)) {
            continue; // Skip if no valid key
        }
        
        sec.data.resize(static_cast<size_t>(sec.size));
        
        // Decrypt based on crypto type
        bool ok = false;
        switch (sec.crypto) {
            case NcaCryptoType::NONE:
                // Plain copy
                if (sec.offset + sec.size <= nca_size) {
                    std::memcpy(sec.data.data(), nca + sec.offset, static_cast<size_t>(sec.size));
                    ok = true;
                }
                break;
                
            case NcaCryptoType::XTS:
                // XTS per sector (not typically used for sections, but handle it)
                // Similar to header but with different key slots
                // For now, fall through to CTR as fallback
                [[fallthrough]];
                
            case NcaCryptoType::CTR:
            case NcaCryptoType::CTR_EX:
                ok = ctrDecryptSection(km, key_slot, nca, nca_size, sec, sec.data.data());
                break;
                
            default:
                ok = false;
        }
        
        if (!ok) {
            sec.valid = false;
            sec.data.clear();
        }
    }
    
    out.valid = true;
    return true;
}

// Convenience: decrypt and extract ExeFS (section 0 typically)
bool decryptNcaExeFS(const KeyManager& km, const uint8_t* nca, size_t nca_size,
                     int header_key_slot, const int section_key_slots[4],
                     std::vector<uint8_t>& out_exefs) {
    NcaDecryptResult result;
    if (!decryptNcaWithKeys(km, nca, nca_size, header_key_slot, section_key_slots, result)) {
        return false;
    }
    
    // ExeFS is typically section 0 for program NCAs
    for (int i = 0; i < 4; i++) {
        if (result.sections[i].valid && result.sections[i].fs_type == NcaFsType::PFS0) {
            out_exefs = std::move(result.sections[i].data);
            return true;
        }
    }
    return false;
}

// Convenience: decrypt and extract RomFS (section 1 typically)
bool decryptNcaRomFS(const KeyManager& km, const uint8_t* nca, size_t nca_size,
                     int header_key_slot, const int section_key_slots[4],
                     std::vector<uint8_t>& out_romfs) {
    NcaDecryptResult result;
    if (!decryptNcaWithKeys(km, nca, nca_size, header_key_slot, section_key_slots, result)) {
        return false;
    }
    
    // RomFS is typically section 1 for program NCAs
    for (int i = 0; i < 4; i++) {
        if (result.sections[i].valid && result.sections[i].fs_type == NcaFsType::ROMFS) {
            out_romfs = std::move(result.sections[i].data);
            return true;
        }
    }
    return false;
}

} // namespace emu
} // namespace mgd