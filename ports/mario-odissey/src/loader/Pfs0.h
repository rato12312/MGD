#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace port {

// Leitor mínimo de PFS0 (container de NSP/HFS0).
// Layout: magic "PFS0", file_count, string_table_size, padding,
// file entries (16 bytes cada: offset, size, name_offset, ...),
// string table com nomes. Sem cripto no container (conteúdo pode ser NCA).
struct Pfs0Entry {
    std::string name;
    uint32_t offset = 0; // relativo ao início dos dados
    uint32_t size = 0;
};

struct Pfs0 {
    static constexpr uint32_t MAGIC = 0x30465350; // "PFS0" little-endian

    bool valid = false;
    std::vector<Pfs0Entry> files;

    static std::optional<Pfs0> parse(const uint8_t* bytes, size_t len) {
        Pfs0 p;
        if (len < 16 || !bytes) return std::nullopt;
        uint32_t magic = 0, count = 0, strtab_size = 0;
        std::memcpy(&magic, bytes, 4);
        std::memcpy(&count, bytes + 4, 4);
        std::memcpy(&strtab_size, bytes + 8, 4);
        if (magic != MAGIC) return std::nullopt;
        if (count > 4096) return std::nullopt; // sanidade
        size_t entries_end = 16 + static_cast<size_t>(count) * 16;
        if (len < entries_end) return std::nullopt;
        size_t strtab = entries_end;
        if (len < strtab + strtab_size) return std::nullopt;
        auto u32 = [&](size_t off) {
            uint32_t v = 0;
            std::memcpy(&v, bytes + off, 4);
            return v;
        };
        for (uint32_t i = 0; i < count; ++i) {
            size_t e = 16 + i * 16;
            uint32_t off = u32(e);
            uint32_t sz = u32(e + 4);
            uint32_t name_off = u32(e + 8);
            if (name_off >= strtab_size) return std::nullopt;
            const char* name = reinterpret_cast<const char*>(bytes + strtab + name_off);
            size_t maxlen = strtab_size - name_off;
            size_t nlen = 0;
            while (nlen < maxlen && name[nlen] != '\0') ++nlen;
            if (nlen == maxlen) return std::nullopt;
            Pfs0Entry entry;
            entry.name = std::string(name, nlen);
            entry.offset = off;
            entry.size = sz;
            p.files.push_back(entry);
        }
        p.valid = true;
        return p;
    }

    static std::optional<Pfs0> parse(const std::vector<uint8_t>& bytes) {
        return parse(bytes.data(), bytes.size());
    }

    const Pfs0Entry* find(const std::string& name) const {
        for (const auto& f : files) {
            if (f.name == name) return &f;
        }
        return nullptr;
    }
};

} // namespace port
