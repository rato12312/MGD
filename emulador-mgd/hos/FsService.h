#pragma once

// fsp-srv (filesystem) — arquivos virtuais em memória.
// A ROM de verdade entra aqui depois; hoje é stub honesto com leitura real.

// cmd 1 = AddFile: payload = nome '\0' + bytes (para teste/boot).
// cmd 2 = Open: payload = nome; responde id ou 0.
// cmd 3 = Read: payload = id(4) + offset(8) + size(8); responde bytes.
// cmd 4 = Close: payload = id; responde 1/0.
// cmd 5 = List: responde nomes separados por '\0'.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Session.h"

namespace mgd {
namespace hos {

class FsService {
public:
    FsService() = default;

    bool dispatch(const IpcMessage& req, IpcMessage& rep) {
        if (req.cmd == 1) {
            size_t z = 0;
            while (z < req.payload.size() && req.payload[z] != 0) z++;
            if (z >= req.payload.size()) {
                rep.cmd = 0;
                return true;
            }
            std::string name(req.payload.begin(), req.payload.begin() + z);
            std::vector<uint8_t> data(req.payload.begin() + z + 1, req.payload.end());
            files_[name] = data;
            rep.cmd = 1;
            return true;
        }
        if (req.cmd == 2) {
            std::string name(req.payload.begin(), req.payload.end());
            if (files_.find(name) == files_.end()) {
                rep.cmd = 0;
                return true;
            }
            uint32_t id = next_fd_++;
            open_[id] = name;
            rep.cmd = 1;
            rep.payload = {static_cast<uint8_t>(id & 0xFF),
                           static_cast<uint8_t>((id >> 8) & 0xFF),
                           static_cast<uint8_t>((id >> 16) & 0xFF),
                           static_cast<uint8_t>((id >> 24) & 0xFF)};
            return true;
        }
        if (req.cmd == 3) {
            if (req.payload.size() < 20) {
                rep.cmd = 0;
                return true;
            }
            uint32_t id = rd32(req.payload, 0);
            uint64_t off = rd64(req.payload, 4);
            uint64_t size = rd64(req.payload, 12);
            auto oit = open_.find(id);
            if (oit == open_.end()) {
                rep.cmd = 0;
                return true;
            }
            const std::vector<uint8_t>& f = files_[oit->second];
            if (off >= f.size()) {
                rep.cmd = 1;
                return true; // EOF: ok vazio
            }
            uint64_t n = size;
            if (off + n > f.size()) n = f.size() - off;
            rep.cmd = 1;
            rep.payload = std::vector<uint8_t>(f.begin() + off, f.begin() + off + n);
            return true;
        }
        if (req.cmd == 4) {
            if (req.payload.size() < 4) {
                rep.cmd = 0;
                return true;
            }
            rep.cmd = open_.erase(rd32(req.payload, 0)) > 0 ? 1 : 0;
            return true;
        }
        if (req.cmd == 5) {
            rep.cmd = 1;
            for (const auto& kv : files_) {
                for (char ch : kv.first) rep.payload.push_back(static_cast<uint8_t>(ch));
                rep.payload.push_back(0);
            }
            return true;
        }
        return false;
    }

    size_t fileCount() const { return files_.size(); }

private:
    static uint32_t rd32(const std::vector<uint8_t>& v, size_t o) {
        uint32_t r = 0;
        for (int i = 0; i < 4; i++) r |= static_cast<uint32_t>(v[o + i]) << (8 * i);
        return r;
    }
    static uint64_t rd64(const std::vector<uint8_t>& v, size_t o) {
        uint64_t r = 0;
        for (int i = 0; i < 8; i++) r |= static_cast<uint64_t>(v[o + i]) << (8 * i);
        return r;
    }

    std::unordered_map<std::string, std::vector<uint8_t>> files_;
    std::unordered_map<uint32_t, std::string> open_;
    uint32_t next_fd_ = 1;
};

} // namespace hos
} // namespace mgd
