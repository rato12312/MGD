#pragma once

// AES-128 ECB (bloco) + CTR (stream, para o NCA).
// Implementação direta do FIPS-197, testada com vetores NIST.
// Chaves do usuário entram aqui depois; nenhuma chave embarcada.

#include <cstdint>
#include <cstring>
#include <vector>

namespace mgd {
namespace emu {
namespace aes {

namespace detail {
inline uint8_t xtime(uint8_t x) { return (x & 0x80) ? (x << 1) ^ 0x1B : (x << 1); }

static const uint8_t kSbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};

inline void expandKey(const uint8_t key[16], uint8_t rk[176]) {
    std::memcpy(rk, key, 16);
    uint8_t rcon = 1;
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4] = {rk[i - 4], rk[i - 3], rk[i - 2], rk[i - 1]};
        if (i % 16 == 0) {
            uint8_t u = t[0];
            t[0] = kSbox[t[1]] ^ rcon;
            t[1] = kSbox[t[2]];
            t[2] = kSbox[t[3]];
            t[3] = kSbox[u];
            rcon = xtime(rcon);
        }
        rk[i] = rk[i - 16] ^ t[0];
        rk[i + 1] = rk[i - 15] ^ t[1];
        rk[i + 2] = rk[i - 14] ^ t[2];
        rk[i + 3] = rk[i - 13] ^ t[3];
    }
}

inline void encryptBlock(const uint8_t rk[176], const uint8_t in[16], uint8_t out[16]) {
    uint8_t s[16];
    for (int i = 0; i < 16; i++) s[i] = in[i] ^ rk[i];
    for (int round = 1; round <= 10; round++) {
        for (int i = 0; i < 16; i++) s[i] = kSbox[s[i]];
        uint8_t t[16]; // ShiftRows
        t[0] = s[0]; t[4] = s[4]; t[8] = s[8]; t[12] = s[12];
        t[1] = s[5]; t[5] = s[9]; t[9] = s[13]; t[13] = s[1];
        t[2] = s[10]; t[6] = s[14]; t[10] = s[2]; t[14] = s[6];
        t[3] = s[15]; t[7] = s[3]; t[11] = s[7]; t[15] = s[11];
        if (round < 10) { // MixColumns
            for (int c = 0; c < 4; c++) {
                uint8_t a0 = t[4 * c], a1 = t[4 * c + 1], a2 = t[4 * c + 2], a3 = t[4 * c + 3];
                uint8_t x0 = xtime(a0), x1 = xtime(a1), x2 = xtime(a2), x3 = xtime(a3);
                s[4 * c] = x0 ^ (x1 ^ a1) ^ a2 ^ a3;
                s[4 * c + 1] = a0 ^ x1 ^ (x2 ^ a2) ^ a3;
                s[4 * c + 2] = a0 ^ a1 ^ x2 ^ (x3 ^ a3);
                s[4 * c + 3] = (x0 ^ a0) ^ a1 ^ a2 ^ x3;
            }
        } else {
            for (int i = 0; i < 16; i++) s[i] = t[i];
        }
        for (int i = 0; i < 16; i++) s[i] ^= rk[round * 16 + i];
    }
    std::memcpy(out, s, 16);
}
} // namespace detail

// ECB de 1 bloco (base do CTR + vetor NIST).
inline void encryptEcb(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    uint8_t rk[176];
    detail::expandKey(key, rk);
    detail::encryptBlock(rk, in, out);
}

// CMAC (SP 800-38B): autentica blocos (cabeçalho NCA usa).
inline void cmac(const uint8_t key[16], const uint8_t* msg, size_t len, uint8_t out[16]) {
    uint8_t rk[176];
    detail::expandKey(key, rk);
    uint8_t L[16] = {0};
    detail::encryptBlock(rk, L, L);
    uint8_t K1[16], K2[16];
    bool msb = (L[0] & 0x80) != 0;
    for (int i = 0; i < 15; i++) K1[i] = (L[i] << 1) | (L[i + 1] >> 7);
    K1[15] = (L[15] << 1) ^ (msb ? 0x87 : 0);
    msb = (K1[0] & 0x80) != 0;
    for (int i = 0; i < 15; i++) K2[i] = (K1[i] << 1) | (K1[i + 1] >> 7);
    K2[15] = (K1[15] << 1) ^ (msb ? 0x87 : 0);
    size_t nblocks = len == 0 ? 1 : (len + 15) / 16;
    bool last_complete = len > 0 && (len % 16 == 0);
    uint8_t X[16] = {0};
    for (size_t b = 0; b + 1 < nblocks; b++) {
        for (int i = 0; i < 16; i++) X[i] ^= msg[b * 16 + i];
        detail::encryptBlock(rk, X, X);
    }
    uint8_t last[16] = {0};
    if (last_complete) {
        for (int i = 0; i < 16; i++) last[i] = msg[(nblocks - 1) * 16 + i] ^ K1[i];
    } else {
        size_t rem = len - (nblocks - 1) * 16;
        for (size_t i = 0; i < rem; i++) last[i] = msg[(nblocks - 1) * 16 + i];
        last[rem] = 0x80;
        for (int i = 0; i < 16; i++) last[i] ^= K2[i];
    }
    for (int i = 0; i < 16; i++) X[i] ^= last[i];
    detail::encryptBlock(rk, X, out);
}

// CTR 128-bit completo (carrega o contador inteiro, BE).
inline void cryptCtrFull(const uint8_t key[16], const uint8_t ctr0[16],
                         const uint8_t* in, uint8_t* out, size_t len) {
    uint8_t rk[176];
    detail::expandKey(key, rk);
    uint8_t ctr[16];
    std::memcpy(ctr, ctr0, 16);
    size_t pos = 0;
    while (pos < len) {
        uint8_t ks[16];
        detail::encryptBlock(rk, ctr, ks);
        size_t n = len - pos < 16 ? len - pos : 16;
        for (size_t i = 0; i < n; i++) out[pos + i] = in[pos + i] ^ ks[i];
        pos += n;
        for (int i = 15; i >= 0; i--) { // ++ BE 128-bit
            ctr[i]++;
            if (ctr[i] != 0) break;
        }
    }
}

// XTS-AES-128 ENCRYPT (SP 800-38E): 2 chaves, tweak por setor (u128 LE).
// Só blocos cheios (header NCA tem 0xC00 % 16 == 0). Decrypt exige a
// cifra inversa (passo futuro). Tweak do NCA tem endianness própria (wiki).
inline bool xtsEncrypt(const uint8_t key1[16], const uint8_t key2[16],
                       const uint8_t tweak16[16], const uint8_t* in,
                       uint8_t* out, size_t len) {
    if (len % 16 != 0) return false;
    uint8_t rk1[176], rk2[176];
    detail::expandKey(key1, rk1);
    detail::expandKey(key2, rk2);
    // T0 = E_k2(tweak); depois multiplica por x a cada bloco
    uint8_t T[16];
    detail::encryptBlock(rk2, tweak16, T);
    size_t pos = 0;
    while (pos < len) {
        uint8_t buf[16];
        for (size_t i = 0; i < 16; i++) buf[i] = in[pos + i] ^ T[i];
        uint8_t enc[16] = {0};
        detail::encryptBlock(rk1, buf, enc);
        for (size_t i = 0; i < 16; i++) out[pos + i] = enc[i] ^ T[i];
        pos += 16;
        // T *= x no corpo GF(2^128) (poly x^128+x^7+x^2+x+1)
        uint8_t carry = (T[15] & 0x80) ? 0x87 : 0;
        for (int i = 15; i > 0; i--) T[i] = (T[i] << 1) | (T[i - 1] >> 7);
        T[0] <<= 1;
        T[0] ^= carry;
    }
    return true;
}

// CTR: keystream = E(nonce||ctr BE), XOR nos dados. Criptografa = descriptografa.
inline void cryptCtr(const uint8_t key[16], const uint8_t nonce12[12], const uint8_t* in,
                     uint8_t* out, size_t len, uint32_t ctr0 = 0) {
    uint8_t rk[176];
    detail::expandKey(key, rk);
    uint8_t ctr[16];
    std::memcpy(ctr, nonce12, 12);
    uint32_t ctrv = ctr0;
    size_t pos = 0;
    while (pos < len) {
        ctr[12] = static_cast<uint8_t>(ctrv >> 24);
        ctr[13] = static_cast<uint8_t>(ctrv >> 16);
        ctr[14] = static_cast<uint8_t>(ctrv >> 8);
        ctr[15] = static_cast<uint8_t>(ctrv);
        uint8_t ks[16];
        detail::encryptBlock(rk, ctr, ks);
        size_t n = len - pos < 16 ? len - pos : 16;
        for (size_t i = 0; i < n; i++) out[pos + i] = in[pos + i] ^ ks[i];
        pos += n;
        ctrv++;
    }
}

} // namespace aes
} // namespace emu
} // namespace mgd
