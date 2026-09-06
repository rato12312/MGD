#pragma once

// RAM do guest: vetor flat com acesso checado (falha fechada).
// O mapa mental mora no disco; a execução mora aqui.

#include <cstdint>
#include <vector>

namespace mgd {
namespace emu {

class GuestRam {
public:
    explicit GuestRam(uint64_t size = 64 * 1024) : mem_(static_cast<size_t>(size), 0) {}

    uint64_t size() const { return static_cast<uint64_t>(mem_.size()); }
    uint8_t* data() { return mem_.data(); }
    const uint8_t* data() const { return mem_.data(); }

    bool write32(uint64_t addr, uint32_t v) {
        if (addr + 4 > size()) return false;
        for (int i = 0; i < 4; i++)
            mem_[static_cast<size_t>(addr) + i] = static_cast<uint8_t>(v >> (8 * i));
        return true;
    }

    bool read32(uint64_t addr, uint32_t& out) const {
        if (addr + 4 > size()) return false;
        out = 0;
        for (int i = 0; i < 4; i++)
            out |= static_cast<uint32_t>(mem_[static_cast<size_t>(addr) + i]) << (8 * i);
        return true;
    }

    bool write64(uint64_t addr, uint64_t v) {
        if (addr + 8 > size()) return false;
        for (int i = 0; i < 8; i++)
            mem_[static_cast<size_t>(addr) + i] = static_cast<uint8_t>(v >> (8 * i));
        return true;
    }

    bool read64(uint64_t addr, uint64_t& out) const {
        if (addr + 8 > size()) return false;
        out = 0;
        for (int i = 0; i < 8; i++)
            out |= static_cast<uint64_t>(mem_[static_cast<size_t>(addr) + i]) << (8 * i);
        return true;
    }

private:
    std::vector<uint8_t> mem_;
};

} // namespace emu
} // namespace mgd
