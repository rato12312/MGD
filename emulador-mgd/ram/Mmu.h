#pragma once

// MMU mínima: regiões VA -> PA com permissão R/W/X.
// Hoje identidade + checagem; paginação real vem depois.
// Falha fechada: fora de região ou sem permissão nega.

#include <cstdint>
#include <vector>

namespace mgd {
namespace emu {

struct MemRegion {
    uint64_t va_base = 0;
    uint64_t pa_base = 0;
    uint64_t size = 0;
    bool r = true, w = true, x = false;
};

class Mmu {
public:
    void map(uint64_t va, uint64_t pa, uint64_t size, bool r = true, bool w = true, bool x = false) {
        regions_.push_back(MemRegion{va, pa, size, r, w, x});
    }
    void unmap(uint64_t va) {
        for (size_t i = 0; i < regions_.size(); i++) {
            if (regions_[i].va_base == va) {
                regions_[i] = regions_.back();
                regions_.pop_back();
                return;
            }
        }
    }

    // Traduz VA -> PA checando permissão (need_w/need_x). false = fault.
    bool translate(uint64_t va, uint64_t accessSize, bool needW, bool needX, uint64_t& paOut) const {
        for (const auto& r : regions_) {
            if (va >= r.va_base && va + accessSize <= r.va_base + r.size) {
                if (needW && !r.w) return false;
                if (needX && !r.x) return false;
                if (!needW && !needX && !r.r) return false;
                paOut = r.pa_base + (va - r.va_base);
                return true;
            }
        }
        return false;
    }

    size_t regionCount() const { return regions_.size(); }

private:
    std::vector<MemRegion> regions_;
};

} // namespace emu
} // namespace mgd
