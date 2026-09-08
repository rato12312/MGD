#pragma once

// Leitor PFS0 (container do NSP): magic, tabela de arquivos, dados.
// file entry: data_offset(8) size(8) name_off(4) reserved(4).

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mgd {
namespace emu {

class Pfs0Reader {
public:
    Pfs0Reader() = default;

    bool open(const uint8_t* blob, size_t len) {
        blob_ = blob;
        len_ = len;
        if (len < 16) return false;
        if (std::memcmp(blob, "PFS0", 4) != 0) return false;
        uint32_t nfiles = rd32(4);
        uint32_t strtab_size = rd32(8);
        header_size_ = 16 + nfiles * 24u;
        strtab_off_ = header_size_;
        data_off_ = header_size_ + strtab_size;
        if (data_off_ > len) return false;
        for (uint32_t i = 0; i < nfiles; i++) {
            size_t e = 16 + i * 24u;
            if (e + 24 > len) return false;
            Entry en;
            en.data_offset = rd64(e);
            en.size = rd64(e + 8);
            uint32_t name_off = rd32(e + 16);
            if (strtab_off_ + name_off >= len) return false;
            en.name = reinterpret_cast<const char*>(blob + strtab_off_ + name_off);
            if (data_off_ + en.data_offset + en.size > len) return false;
            files_.push_back(en);
        }
        return true;
    }

    size_t fileCount() const { return files_.size(); }

    std::string fileName(size_t i) const {
        return i < files_.size() ? files_[i].name : "";
    }

    bool readFile(const std::string& name, std::vector<uint8_t>& out) const {
        for (const auto& f : files_) {
            if (f.name == name) {
                out.assign(blob_ + data_off_ + f.data_offset,
                           blob_ + data_off_ + f.data_offset + f.size);
                return true;
            }
        }
        return false;
    }

    // Atalhos ExeFS (wiki: é PFS0 com estes nomes).
    bool readMain(std::vector<uint8_t>& out) const { return readFile("main", out); }
    bool readNpdm(std::vector<uint8_t>& out) const { return readFile("main.npdm", out); }

private:
    struct Entry {
        std::string name;
        uint64_t data_offset = 0;
        uint64_t size = 0;
    };
    uint32_t rd32(size_t off) const {
        uint32_t v = 0;
        std::memcpy(&v, blob_ + off, 4);
        return v;
    }
    uint64_t rd64(size_t off) const {
        uint64_t v = 0;
        std::memcpy(&v, blob_ + off, 8);
        return v;
    }

    const uint8_t* blob_ = nullptr;
    size_t len_ = 0;
    uint32_t header_size_ = 0, strtab_off_ = 0;
    uint64_t data_off_ = 0;
    std::vector<Entry> files_;
};

} // namespace emu
} // namespace mgd
