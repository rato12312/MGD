#pragma once

// HIPC mínimo: monta/parce cabeçalho de pedido e resposta.
// Pedido: "SFCI"(0) ver u32(4) cmd u32(8). Resposta: "SFCO"(0) + result u64(8).
// Buffers (X/A/B/W) entram no próximo passo; hoje é comando puro.
// Se dump real discordar do layout, ajusta aqui (ponto único).

#include <cstdint>
#include <cstring>
#include <vector>

namespace mgd {
namespace hos {

struct HipcRequest {
    uint32_t cmd = 0;
};

struct HipcReply {
    uint64_t result = 0;
};

inline std::vector<uint8_t> hipcMakeRequest(uint32_t cmd) {
    std::vector<uint8_t> b(12, 0);
    b[0] = 'S';
    b[1] = 'F';
    b[2] = 'C';
    b[3] = 'I';
    // ver(4..7) = 0
    std::memcpy(b.data() + 8, &cmd, 4);
    return b;
}

inline bool hipcParseRequest(const std::vector<uint8_t>& b, HipcRequest& out) {
    if (b.size() < 12) return false;
    if (b[0] != 'S' || b[1] != 'F' || b[2] != 'C' || b[3] != 'I') return false;
    std::memcpy(&out.cmd, b.data() + 8, 4);
    return true;
}

inline std::vector<uint8_t> hipcMakeReply(uint64_t result) {
    std::vector<uint8_t> b(16, 0);
    b[0] = 'S';
    b[1] = 'F';
    b[2] = 'C';
    b[3] = 'O';
    std::memcpy(b.data() + 8, &result, 8);
    return b;
}

inline bool hipcParseReply(const std::vector<uint8_t>& b, HipcReply& out) {
    if (b.size() < 16) return false;
    if (b[0] != 'S' || b[1] != 'F' || b[2] != 'C' || b[3] != 'O') return false;
    std::memcpy(&out.result, b.data() + 8, 8);
    return true;
}

} // namespace hos
} // namespace mgd
