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
        __builtin_memcpy(&mem_[static_cast<size_t>(addr)], &v, 4);
        return true;
    }

    bool read32(uint64_t addr, uint32_t& out) const {
        if (addr + 4 > size()) return false;
        __builtin_memcpy(&out, &mem_[static_cast<size_t>(addr)], 4);
        return true;
    }

    bool write64(uint64_t addr, uint64_t v) {
        if (addr + 8 > size()) return false;
        __builtin_memcpy(&mem_[static_cast<size_t>(addr)], &v, 8);
        return true;
    }

    bool read64(uint64_t addr, uint64_t& out) const {
        if (addr + 8 > size()) return false;
        __builtin_memcpy(&out, &mem_[static_cast<size_t>(addr)], 8);
        return true;
    }

private:
    std::vector<uint8_t> mem_;
};

} // namespace emu
} // namespace mgd
