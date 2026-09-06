#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace port {

// Descompressor Yaz0 (SZS do Switch). Formato documentado e estável:
// magic "Yaz0", tamanho descomprimido em +4, depois grupos de 8 símbolos
// com byte de controle (1 = literal, 0 = match offset/length).
// Sem dependência do jogo: testável com bytes sintéticos.
class Yaz0 {
public:
    static std::optional<std::vector<uint8_t>> decompress(const uint8_t* bytes, size_t len) {
        if (len < 16 || !bytes) return std::nullopt;
        if (!(bytes[0] == 'Y' && bytes[1] == 'a' && bytes[2] == 'z' && bytes[3] == '0')) {
            return std::nullopt;
        }
        uint32_t out_size = (static_cast<uint32_t>(bytes[4]) << 24) |
                            (static_cast<uint32_t>(bytes[5]) << 16) |
                            (static_cast<uint32_t>(bytes[6]) << 8) |
                            static_cast<uint32_t>(bytes[7]);
        if (out_size == 0 || out_size > (32u << 20)) return std::nullopt; // sanidade: 32MB
        std::vector<uint8_t> out;
        out.reserve(out_size);
        size_t pos = 16;
        while (out.size() < out_size) {
            if (pos >= len) return std::nullopt;
            uint8_t control = bytes[pos++];
            for (int bit = 7; bit >= 0 && out.size() < out_size; --bit) {
                if (control & (1u << bit)) {
                    if (pos >= len) return std::nullopt;
                    out.push_back(bytes[pos++]);
                } else {
                    if (pos + 1 >= len) return std::nullopt;
                    uint8_t b0 = bytes[pos++];
                    uint8_t b1 = bytes[pos++];
                    size_t dist = (((b0 & 0x0F) << 8) | b1) + 1;
                    size_t count = (b0 >> 4) + 2;
                    if (b0 >> 4 == 0) {
                        if (pos >= len) return std::nullopt;
                        count = static_cast<size_t>(bytes[pos++]) + 0x12;
                    }
                    if (dist == 0 || dist > out.size()) return std::nullopt;
                    size_t src = out.size() - dist;
                    for (size_t i = 0; i < count && out.size() < out_size; ++i) {
                        out.push_back(out[src + i]);
                    }
                }
            }
        }
        return out;
    }

    static std::optional<std::vector<uint8_t>> decompress(const std::vector<uint8_t>& bytes) {
        return decompress(bytes.data(), bytes.size());
    }
};

} // namespace port
