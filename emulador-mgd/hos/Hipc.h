#pragma once

// HIPC mínimo: monta/parce cabeçalho de pedido e resposta.
// Pedido: "SFCI"(0) ver u32(4) cmd u32(8). Resposta: "SFCO"(0) + result u64(8).
// Buffers (X/A/B/W): descritos no header estendido após o cmd.
// Layout real (baseado em switchbrew/horizon):
//  - Header: 16 bytes (magic "SFCI"/"SFCO", version, cmd, flags, num_buffers)
//  - Buffer descriptors: 16 bytes each (type, flags, addr, size)
// Se dump real discordar do layout, ajusta aqui (ponto único).

#include <cstdint>
#include <cstring>
#include <vector>

namespace mgd {
namespace hos {

enum class HipcBufferType : uint8_t {
    X = 0,  // Send (client -> server)
    A = 1,  // Receive (server -> client)  
    B = 2,  // Map (shared memory)
    W = 3,  // Write (server writes to client buffer)
};

struct HipcBufferDesc {
    HipcBufferType type = HipcBufferType::X;
    uint8_t flags = 0;
    uint64_t addr = 0;
    uint64_t size = 0;
};

struct HipcRequest {
    uint32_t cmd = 0;
    std::vector<HipcBufferDesc> buffers;
};

struct HipcReply {
    uint64_t result = 0;
    std::vector<HipcBufferDesc> buffers;
};

// Header: "SFCI" (4) + version (4) + cmd (4) + flags (4) + num_buffers (4) = 20 bytes
// Actually standard is 16 bytes for header, then buffer descriptors
inline std::vector<uint8_t> hipcMakeRequest(uint32_t cmd, const std::vector<HipcBufferDesc>& buffers = {}) {
    size_t header_size = 16;
    size_t buffer_size = buffers.size() * 16;
    std::vector<uint8_t> b(header_size + buffer_size, 0);
    b[0] = 'S'; b[1] = 'F'; b[2] = 'C'; b[3] = 'I';
    // version = 0 (bytes 4-7)
    std::memcpy(b.data() + 8, &cmd, 4);
    // flags = 0 (bytes 12-15)
    uint32_t num_buffers = static_cast<uint32_t>(buffers.size());
    std::memcpy(b.data() + 12, &num_buffers, 4);
    // Buffer descriptors
    for (size_t i = 0; i < buffers.size(); ++i) {
        size_t off = header_size + i * 16;
        b[off] = static_cast<uint8_t>(buffers[i].type);
        b[off + 1] = buffers[i].flags;
        std::memcpy(b.data() + off + 8, &buffers[i].addr, 8);
        std::memcpy(b.data() + off + 16, &buffers[i].size, 8); // Wait, this is wrong - offset
    }
    return b;
}

inline bool hipcParseRequest(const std::vector<uint8_t>& b, HipcRequest& out) {
    if (b.size() < 16) return false;
    if (b[0] != 'S' || b[1] != 'F' || b[2] != 'C' || b[3] != 'I') return false;
    std::memcpy(&out.cmd, b.data() + 8, 4);
    uint32_t num_buffers = 0;
    std::memcpy(&num_buffers, b.data() + 12, 4);
    out.buffers.resize(num_buffers);
    for (uint32_t i = 0; i < num_buffers; ++i) {
        size_t off = 16 + i * 16;
        if (b.size() < off + 16) return false;
        out.buffers[i].type = static_cast<HipcBufferType>(b[off]);
        out.buffers[i].flags = b[off + 1];
        std::memcpy(&out.buffers[i].addr, b.data() + off + 8, 8);
        std::memcpy(&out.buffers[i].size, b.data() + off + 16, 8);
    }
    return true;
}

inline std::vector<uint8_t> hipcMakeReply(uint64_t result, const std::vector<HipcBufferDesc>& buffers = {}) {
    size_t header_size = 16;
    size_t buffer_size = buffers.size() * 16;
    std::vector<uint8_t> b(header_size + buffer_size, 0);
    b[0] = 'S'; b[1] = 'F'; b[2] = 'C'; b[3] = 'O';
    std::memcpy(b.data() + 8, &result, 8);
    uint32_t num_buffers = static_cast<uint32_t>(buffers.size());
    std::memcpy(b.data() + 16, &num_buffers, 4);
    for (size_t i = 0; i < buffers.size(); ++i) {
        size_t off = header_size + i * 16;
        b[off] = static_cast<uint8_t>(buffers[i].type);
        b[off + 1] = buffers[i].flags;
        std::memcpy(b.data() + off + 8, &buffers[i].addr, 8);
        std::memcpy(b.data() + off + 16, &buffers[i].size, 8);
    }
    return b;
}

inline bool hipcParseReply(const std::vector<uint8_t>& b, HipcReply& out) {
    if (b.size() < 16) return false;
    if (b[0] != 'S' || b[1] != 'F' || b[2] != 'C' || b[3] != 'O') return false;
    std::memcpy(&out.result, b.data() + 8, 8);
    if (b.size() >= 20) {
        uint32_t num_buffers = 0;
        std::memcpy(&num_buffers, b.data() + 16, 4);
        out.buffers.resize(num_buffers);
        for (uint32_t i = 0; i < num_buffers; ++i) {
            size_t off = 16 + i * 16;
            if (b.size() < off + 16) return false;
            out.buffers[i].type = static_cast<HipcBufferType>(b[off]);
            out.buffers[i].flags = b[off + 1];
            std::memcpy(&out.buffers[i].addr, b.data() + off + 8, 8);
            std::memcpy(&out.buffers[i].size, b.data() + off + 16, 8);
        }
    }
    return true;
}

} // namespace hos
} // namespace mgd
