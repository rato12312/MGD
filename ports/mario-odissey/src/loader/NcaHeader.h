#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace port {

// Leitor mínimo de cabeçalho NCA (container do Switch).
// Só valida magic "NCA3" e extrai distribuição/tipo e offsets das seções.
// Conteúdo é criptografado (AES-XTS): descriptografia fica como TODO
// exigindo as keys do console do usuário. Testável com bytes sintéticos.
struct NcaSection {
    uint32_t media_offset = 0;
    uint32_t media_end_offset = 0;
};

struct NcaHeader {
    static constexpr size_t SIZE = 0x400;
    static constexpr size_t MAGIC_OFFSET = 0x200;
    static constexpr uint32_t MAGIC = 0x3341434E; // "NCA3" little-endian

    bool valid = false;
    uint8_t distribution = 0; // 0 = download, 1 = gamecard
    uint8_t content_type = 0; // 0 = program, 1 = meta, ...
    NcaSection sections[4];

    static std::optional<NcaHeader> parse(const uint8_t* bytes, size_t len) {
        NcaHeader h;
        if (len < SIZE || !bytes) return std::nullopt;
        uint32_t magic = 0;
        std::memcpy(&magic, bytes + MAGIC_OFFSET, 4);
        if (magic != MAGIC) return std::nullopt;
        h.distribution = bytes[MAGIC_OFFSET + 4];
        h.content_type = bytes[MAGIC_OFFSET + 5];
        auto u32 = [&](size_t off) {
            uint32_t v = 0;
            std::memcpy(&v, bytes + off, 4);
            return v;
        };
        for (int i = 0; i < 4; ++i) {
            h.sections[i].media_offset = u32(0x240 + i * 0x10);
            h.sections[i].media_end_offset = u32(0x244 + i * 0x10);
        }
        h.valid = true;
        return h;
    }

    static std::optional<NcaHeader> parse(const std::vector<uint8_t>& bytes) {
        return parse(bytes.data(), bytes.size());
    }
};

} // namespace port
