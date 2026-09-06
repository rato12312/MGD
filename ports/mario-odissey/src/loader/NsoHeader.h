#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace port {

// Leitor mínimo de cabeçalho NSO (executável do Switch).
// Só valida magic "NSO0" e extrai segmentos + Build ID.
// Sem dependência de jogo real: testável com bytes sintéticos.
struct NsoSegment {
    uint32_t file_offset = 0;
    uint32_t memory_offset = 0;
    uint32_t decompressed_size = 0;
};

struct NsoHeader {
    static constexpr size_t SIZE = 0x100;
    static constexpr uint32_t MAGIC = 0x304F534E; // "NSO0" little-endian

    bool valid = false;
    uint32_t version = 0;
    NsoSegment text, rodata, data;
    uint32_t bss_size = 0;
    uint8_t build_id[32] = {};

    static std::optional<NsoHeader> parse(const uint8_t* bytes, size_t len) {
        NsoHeader h;
        if (len < SIZE || !bytes) return std::nullopt;
        uint32_t magic = 0;
        std::memcpy(&magic, bytes, 4);
        if (magic != MAGIC) return std::nullopt;
        auto u32 = [&](size_t off) {
            uint32_t v = 0;
            std::memcpy(&v, bytes + off, 4);
            return v;
        };
        h.version = u32(4);
        h.text = {u32(0x10), u32(0x14), u32(0x18)};
        h.rodata = {u32(0x20), u32(0x24), u32(0x28)};
        h.data = {u32(0x30), u32(0x34), u32(0x38)};
        h.bss_size = u32(0x3C);
        std::memcpy(h.build_id, bytes + 0x40, 32);
        h.valid = true;
        return h;
    }

    static std::optional<NsoHeader> parse(const std::vector<uint8_t>& bytes) {
        return parse(bytes.data(), bytes.size());
    }
};

} // namespace port
