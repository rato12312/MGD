#pragma once

// Chaveiro: slots de chave AES-128 setados pelo USUÁRIO (nunca embarcadas).
// Descriptografa seção com CTR 128-bit explícito (sem adivinhar layout NCA).

#include <cstdint>
#include <cstring>
#include <vector>

#include "Aes.h"

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

    // Criptografa/descriptografa seção inteira (CTR é simétrico).
    bool cryptSection(const uint8_t* in, uint8_t* out, size_t len, int slot,
                      const uint8_t ctr[16]) const {
        if (!hasSlot(slot)) return false;
        aes::cryptCtrFull(keys_[slot], ctr, in, out, len);
        return true;
    }

private:
    uint8_t keys_[SLOTS][16] = {};
    bool have_[SLOTS] = {};
};

} // namespace emu
} // namespace mgd
