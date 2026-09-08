#pragma once

// Leitor RomFS: tabelas de diretório/arquivo + dados.
// Offsets absolutos a partir da base do RomFS (se dump real discordar,
// ajusta a base aqui — formato segue a switchbrew).
// Irmãos: sibling liga, como manda a filosofia (location + sibling).

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mgd {
namespace emu {

class RomFsReader {
public:
    RomFsReader() = default;

    bool open(const uint8_t* blob, size_t len) {
        blob_ = blob;
        len_ = len;
        if (len < 0x50) return false;
        dir_hash_off_ = rd32(0x04);
        dir_tab_off_ = rd32(0x0C);
        dir_tab_size_ = rd32(0x10);
        file_hash_off_ = rd32(0x14);
        file_tab_off_ = rd32(0x1C);
        file_tab_size_ = rd32(0x20);
        data_off_ = rd64(0x24);
        if (dir_tab_off_ + dir_tab_size_ > len) return false;
        if (file_tab_off_ + file_tab_size_ > len) return false;
        if (data_off_ > len) return false;
        return true;
    }

    // Lista arquivos do diretório raiz (nomes).
    std::vector<std::string> listRoot() const {
        std::vector<std::string> out;
        if (!blob_) return out;
        uint32_t file = rd32(dir_tab_off_ + 0x0C); // root.file
        while (file != 0xFFFFFFFFu) {
            out.push_back(fileName(file));
            file = rd32(file + 0x04); // sibling
        }
        return out;
    }

    // Subdiretórios (1 nível): layout dir = parent@0 sibling@4 child@8
    // file@12 namelen@0x14 nome@0x18. Convenção documentada aqui.
    std::vector<std::string> listSubdir(const std::string& sub) const {
        std::vector<std::string> out;
        if (!blob_) return out;
        uint32_t dir = rd32(dir_tab_off_ + 0x08); // root.child
        while (dir != 0xFFFFFFFFu) {
            if (dirName(dir) == sub) {
                uint32_t file = rd32(dir + 0x0C);
                while (file != 0xFFFFFFFFu) {
                    out.push_back(fileName(file));
                    file = rd32(file + 0x04);
                }
                return out;
            }
            dir = rd32(dir + 0x04);
        }
        return out;
    }

    bool readSubFile(const std::string& sub, const std::string& name,
                     std::vector<uint8_t>& out) const {
        if (!blob_) return false;
        uint32_t dir = rd32(dir_tab_off_ + 0x08);
        while (dir != 0xFFFFFFFFu) {
            if (dirName(dir) == sub) {
                uint32_t file = rd32(dir + 0x0C);
                while (file != 0xFFFFFFFFu) {
                    if (fileName(file) == name) {
                        uint64_t doff = rd64(file + 0x08);
                        uint64_t dsize = rd64(file + 0x10);
                        if (data_off_ + doff + dsize > len_) return false;
                        out.assign(blob_ + data_off_ + doff, blob_ + data_off_ + doff + dsize);
                        return true;
                    }
                    file = rd32(file + 0x04);
                }
                return false;
            }
            dir = rd32(dir + 0x04);
        }
        return false;
    }

    // Lê arquivo do raiz por nome. false = não achou.
    bool readRootFile(const std::string& name, std::vector<uint8_t>& out) const {
        if (!blob_) return false;
        uint32_t file = rd32(dir_tab_off_ + 0x0C);
        while (file != 0xFFFFFFFFu) {
            if (fileName(file) == name) {
                uint64_t doff = rd64(file + 0x08);
                uint64_t dsize = rd64(file + 0x10);
                if (data_off_ + doff + dsize > len_) return false;
                out.assign(blob_ + data_off_ + doff, blob_ + data_off_ + doff + dsize);
                return true;
            }
            file = rd32(file + 0x04);
        }
        return false;
    }

private:
    std::string dirName(uint32_t entry) const {
        uint32_t nlen = rd32(entry + 0x14);
        if (entry + 0x18 + nlen > len_) return "";
        return std::string(reinterpret_cast<const char*>(blob_ + entry + 0x18), nlen);
    }
    std::string fileName(uint32_t entry) const {
        uint32_t nlen = rd32(entry + 0x1C);
        if (entry + 0x20 + nlen > len_) return "";
        return std::string(reinterpret_cast<const char*>(blob_ + entry + 0x20), nlen);
    }
    uint32_t rd32(size_t off) const {
        uint32_t v = 0;
        if (off + 4 <= len_) std::memcpy(&v, blob_ + off, 4);
        return v;
    }
    uint64_t rd64(size_t off) const {
        uint64_t v = 0;
        if (off + 8 <= len_) std::memcpy(&v, blob_ + off, 8);
        return v;
    }

    const uint8_t* blob_ = nullptr;
    size_t len_ = 0;
    uint32_t dir_hash_off_ = 0, dir_tab_off_ = 0, dir_tab_size_ = 0;
    uint32_t file_hash_off_ = 0, file_tab_off_ = 0, file_tab_size_ = 0;
    uint64_t data_off_ = 0;
};

} // namespace emu
} // namespace mgd
