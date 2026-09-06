#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace port {

// Analyzers de cabeçalho do RomFS do Odyssey (dentro do NSP):
// SARC (arquivo), BYML (parâmetros), BFRES (modelos), BNTX (texturas).
// Só magic + versão, como fizemos com BSA/ESP. Sem dependência do dump real.
struct RomfsInfo {
    std::string format; // "SARC", "BYML", "BFRES", "BNTX"
    uint32_t version = 0;
    size_t file_size = 0;
};

inline std::optional<RomfsInfo> analyzeSarc(const uint8_t* bytes, size_t len, size_t file_size) {
    if (len < 0x14 || !bytes) return std::nullopt;
    if (!(bytes[0] == 'S' && bytes[1] == 'A' && bytes[2] == 'R' && bytes[3] == 'C')) return std::nullopt;
    RomfsInfo info;
    info.format = "SARC";
    info.file_size = file_size;
    return info;
}

inline std::optional<RomfsInfo> analyzeByml(const uint8_t* bytes, size_t len, size_t file_size) {
    if (len < 4 || !bytes) return std::nullopt;
    if (!(bytes[0] == 'B' && bytes[1] == 'Y')) return std::nullopt;
    uint16_t ver = 0;
    std::memcpy(&ver, bytes + 2, 2);
    RomfsInfo info;
    info.format = "BYML";
    info.version = ver;
    info.file_size = file_size;
    return info;
}

inline std::optional<RomfsInfo> analyzeYaz0(const uint8_t* bytes, size_t len, size_t file_size) {
    if (len < 8 || !bytes) return std::nullopt;
    if (!(bytes[0] == 'Y' && bytes[1] == 'a' && bytes[2] == 'z' && bytes[3] == '0')) return std::nullopt;
    RomfsInfo info;
    info.format = "YAZ0";
    info.file_size = file_size;
    return info;
}

inline std::optional<RomfsInfo> analyzeBfres(const uint8_t* bytes, size_t len, size_t file_size) {
    if (len < 8 || !bytes) return std::nullopt;
    if (!(bytes[0] == 'F' && bytes[1] == 'R' && bytes[2] == 'E' && bytes[3] == 'S')) return std::nullopt;
    RomfsInfo info;
    info.format = "BFRES";
    info.file_size = file_size;
    return info;
}

inline std::optional<RomfsInfo> analyzeBntx(const uint8_t* bytes, size_t len, size_t file_size) {
    if (len < 8 || !bytes) return std::nullopt;
    if (!(bytes[0] == 'B' && bytes[1] == 'N' && bytes[2] == 'T' && bytes[3] == 'X')) return std::nullopt;
    RomfsInfo info;
    info.format = "BNTX";
    info.file_size = file_size;
    return info;
}

// Despacha pela extensão típica dentro do NSP/RomFS.
inline std::optional<RomfsInfo> analyzeRomfs(const std::string& ext, const std::vector<uint8_t>& bytes) {
    if (ext == ".szs") return analyzeYaz0(bytes.data(), bytes.size(), bytes.size());
    if (ext == ".sarc") return analyzeSarc(bytes.data(), bytes.size(), bytes.size());
    if (ext == ".byml" || ext == ".byaml") return analyzeByml(bytes.data(), bytes.size(), bytes.size());
    if (ext == ".bfres") return analyzeBfres(bytes.data(), bytes.size(), bytes.size());
    if (ext == ".bntx" || ext == ".dds") return analyzeBntx(bytes.data(), bytes.size(), bytes.size());
    return std::nullopt;
}

} // namespace port
