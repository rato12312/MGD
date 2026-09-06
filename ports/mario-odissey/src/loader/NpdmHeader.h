#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace port {

// Leitor mínimo de NPDM (ex.: main.npdm, de onde sai o heap size no boot).
// Valida magic "META" e tamanho mínimo. Campos detalhados (ACID/ACI,
// permissões) ficam como TODO — exigem dump real para validar offsets.
struct NpdmHeader {
    static constexpr size_t MIN_SIZE = 0x100;
    static constexpr uint32_t MAGIC = 0x4154454D; // "META" little-endian

    bool valid = false;

    static std::optional<NpdmHeader> parse(const uint8_t* bytes, size_t len) {
        NpdmHeader h;
        if (len < MIN_SIZE || !bytes) return std::nullopt;
        uint32_t magic = 0;
        std::memcpy(&magic, bytes, 4);
        if (magic != MAGIC) return std::nullopt;
        h.valid = true;
        return h;
    }

    static std::optional<NpdmHeader> parse(const std::vector<uint8_t>& bytes) {
        return parse(bytes.data(), bytes.size());
    }
};

} // namespace port
