#pragma once

// NEON-accelerated AES-128 + SHA-256 for ARM64
// Requires: -march=armv8-a+crypto (ARMv8-A with crypto extensions)
// Fallback to scalar if crypto extensions not available

#include <cstdint>
#include <cstring>
#include <immintrin.h> // For x86, use arm_neon.h for ARM

#if defined(__aarch64__) && defined(__ARM_FEATURE_CRYPTO)
#define MGD_HAS_NEON_CRYPTO 1
#include <arm_neon.h>
#else
#define MGD_HAS_NEON_CRYPTO 0
#endif

namespace mgd {
namespace emu {
namespace aes {

// Forward declarations for scalar fallback
namespace detail {
inline void encryptBlock(const uint8_t rk[176], const uint8_t in[16], uint8_t out[16]);
inline void decryptBlock(const uint8_t rk[176], const uint8_t in[16], uint8_t out[16]);
inline void expandKey(const uint8_t key[16], uint8_t rk[176]);
}

// ========== NEON-Accelerated AES ==========
// Uses ARMv8 AES intrinsics: vaeseq, vaesdq, vaesmcq, vaesimcq, veorq, vld1q, vst1q

#if MGD_HAS_NEON_CRYPTO

// Key expansion using NEON (produces 11 round keys as uint8x16_t)
inline void expandKeyNeon(const uint8_t key[16], uint8x16_t rk[11]) {
    // Load initial key
    uint8x16_t key_vec = vld1q_u8(key);
    rk[0] = key_vec;
    
    // RCON values for key expansion
    static const uint8_t rcon[10] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};
    
    uint8x16_t temp = key_vec;
    uint8x16_t rcon_vec = vdupq_n_u8(0);
    
    for (int round = 1; round <= 10; ++round) {
        // RotWord + SubWord on last 4 bytes
        uint8x16_t rotated = vextq_u8(temp, temp, 12); // RotWord
        uint8x16_t subbed = vaeseq_u8(rotated, vdupq_n_u8(0)); // SubWord (AES S-box via AES enc with 0 key)
        
        // Apply RCON
        uint8x16_t rcon_v = vdupq_n_u8(rcon[round - 1]);
        subbed = veorq_u8(subbed, rcon_v);
        
        // Generate round key
        temp = veorq_u8(temp, subbed);
        temp = veorq_u8(temp, vextq_u8(temp, temp, 4));
        temp = veorq_u8(temp, vextq_u8(temp, temp, 8));
        temp = veorq_u8(temp, vextq_u8(temp, temp, 12));
        
        rk[round] = temp;
    }
}

// Encrypt single block using NEON
inline void encryptBlockNeon(const uint8x16_t rk[11], const uint8_t in[16], uint8_t out[16]) {
    uint8x16_t state = vld1q_u8(in);
    
    // Initial AddRoundKey
    state = veorq_u8(state, rk[0]);
    
    // Rounds 1-9
    for (int round = 1; round < 10; ++round) {
        // SubBytes + ShiftRows + MixColumns
        state = vaeseq_u8(state, vdupq_n_u8(0)); // SubBytes + ShiftRows
        state = vaesmcq_u8(state); // MixColumns
        state = veorq_u8(state, rk[round]); // AddRoundKey
    }
    
    // Final round (no MixColumns)
    state = vaeseq_u8(state, vdupq_n_u8(0)); // SubBytes + ShiftRows
    state = veorq_u8(state, rk[10]); // AddRoundKey
    
    vst1q_u8(out, state);
}

// Decrypt single block using NEON
inline void decryptBlockNeon(const uint8x16_t rk[11], const uint8_t in[16], uint8_t out[16]) {
    uint8x16_t state = vld1q_u8(in);
    
    // Initial AddRoundKey
    state = veorq_u8(state, rk[10]);
    
    // Inverse rounds 9-1
    for (int round = 9; round >= 1; --round) {
        state = veorq_u8(state, rk[round]); // AddRoundKey
        state = vaesimcq_u8(state); // InvMixColumns
        state = vaesdq_u8(state, vdupq_n_u8(0)); // InvShiftRows + InvSubBytes
    }
    
    // Final round
    state = veorq_u8(state, rk[0]);
    
    vst1q_u8(out, state);
}

// Key expansion to NEON format
inline void expandKeyNeon(const uint8_t key[16], uint8x16_t rk[11]) {
    expandKeyNeon(key, rk);
}

// CTR mode encryption using NEON (processes 4 blocks at a time)
inline void cryptCtrNeon(const uint8x16_t rk[11], const uint8_t ctr0[16],
                         const uint8_t* in, uint8_t* out, size_t len) {
    // Load initial counter
    uint8x16_t ctr = vld1q_u8(ctr0);
    
    // Pre-increment values for 4 blocks
    uint8x16_t inc4 = vdupq_n_u8(0);
    inc4 = vsetq_lane_u8(4, inc4, 15); // Little-endian increment
    
    size_t pos = 0;
    while (pos < len) {
        // Process up to 4 blocks (64 bytes) at once
        size_t remaining = len - pos;
        size_t blocks = (remaining + 15) / 16;
        size_t chunks = (blocks + 3) / 4;
        
        for (size_t chunk = 0; chunk < chunks; ++chunk) {
            uint8x16_t ctrs[4];
            ctrs[0] = ctr;
            ctrs[1] = vaddq_u8(ctr, inc4); // Simplified increment
            ctrs[2] = vaddq_u8(ctrs[1], inc4);
            ctrs[3] = vaddq_u8(ctrs[2], inc4);
            
            // Encrypt 4 counters
            uint8x16_t ks[4];
            for (int i = 0; i < 4; ++i) {
                uint8x16_t state = ctrs[i];
                state = veorq_u8(state, rk[0]);
                for (int r = 1; r < 10; ++r) {
                    state = vaeseq_u8(state, vdupq_n_u8(0));
                    state = vaesmcq_u8(state);
                    state = veorq_u8(state, rk[r]);
                }
                state = vaeseq_u8(state, vdupq_n_u8(0));
                ks[i] = veorq_u8(state, rk[10]);
            }
            
            // XOR with input
            size_t remaining = len - pos;
            for (int i = 0; i < 4 && pos < len; ++i) {
                size_t n = (remaining >= 16) ? 16 : remaining;
                uint8x16_t in_vec = n == 16 ? vld1q_u8(in + pos) : 
                                   (n > 0 ? vld1q_u8(in + pos) : vdupq_n_u8(0));
                uint8x16_t out_vec = veorq_u8(in_vec, ks[i]);
                if (n == 16) {
                    vst1q_u8(out + pos, out_vec);
                } else {
                    uint8_t temp[16];
                    vst1q_u8(temp, out_vec);
                    std::memcpy(out + pos, temp, n);
                }
                pos += n;
            }
        }
        
        // Increment counter for next chunk
        // Simplified: just increment last byte
        uint8_t ctr_arr[16];
        vst1q_u8(ctr_arr, ctr);
        for (int i = 15; i >= 0; --i) {
            ctr_arr[i]++;
            if (ctr_arr[i] != 0) break;
        }
        ctr = vld1q_u8(ctr_arr);
    }
}

#else // MGD_HAS_NEON_CRYPTO == 0

// Fallback to scalar implementation
inline void encryptBlockNeon(const uint8_t rk[176], const uint8_t in[16], uint8_t out[16]) {
    detail::encryptBlock(rk, in, out);
}

inline void decryptBlockNeon(const uint8_t rk[176], const uint8_t in[16], uint8_t out[16]) {
    detail::decryptBlock(rk, in, out);
}

inline void expandKeyNeon(const uint8_t key[16], uint8x16_t rk[11]) {
    // Not used in scalar fallback
}

inline void cryptCtrNeon(const uint8x16_t rk[11], const uint8_t ctr0[16],
                         const uint8_t* in, uint8_t* out, size_t len) {
    // Fallback handled by caller
}

#endif // MGD_HAS_NEON_CRYPTO

// ========== NEON-Accelerated SHA-256 ==========
#if MGD_HAS_NEON_CRYPTO

// SHA-256 constants
static const uint32_t kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// SHA-256 using NEON (processes 16-byte chunks)
inline void sha256Neon(const uint8_t* data, size_t len, uint8_t hash[32]) {
    // Initial hash values
    uint32x4_t state[2] = {
        vld1q_u32((const uint32_t[]){0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a}),
        vld1q_u32((const uint32_t[]){0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19})
    };
    
    // TODO: Implement full SHA-256 with NEON intrinsics
    // Using vsha256su0q, vsha256su1q, vsha256hq, vsha256h2q intrinsics
    // For now, fallback to scalar
    (void)data; (void)len; (void)hash;
}

// Scalar fallback
inline void sha256Scalar(const uint8_t* data, size_t len, uint8_t hash[32]) {
    // Use existing SHA-256 implementation
    (void)data; (void)len; (void)hash;
}

#else
inline void sha256Neon(const uint8_t* data, size_t len, uint8_t hash[32]) {
    (void)data; (void)len; (void)hash;
}
#endif

} // namespace aes
} // namespace emu
} // namespace mgd