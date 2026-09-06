#pragma once

// Campo de bits estilo MGD: mesma ideia do BitField do Eden,
// escrita do zero sem dependência GPL. Usa-se em union com o u32 cru:
//   union Insn { uint32_t hex; Field<0,5,uint32_t> rd; ... };

#include <cstdint>
#include <type_traits>

namespace port {
namespace mgd {

template <int Pos, int Bits, typename T>
struct Field {
    static_assert(Pos >= 0 && Bits > 0 && Pos + Bits <= 32, "campo fora do u32");
    static_assert(std::is_integral_v<T>, "T precisa ser inteiro");

    static constexpr uint32_t kMask = (Bits == 32) ? 0xFFFFFFFFu
                                                   : ((1u << Bits) - 1u);

    operator T() const {
        uint32_t v = (raw() >> Pos) & kMask;
        if constexpr (std::is_signed_v<T>) {
            // estende o sinal do campo para T
            if (v & (1u << (Bits - 1))) v |= ~kMask;
        }
        return static_cast<T>(v);
    }

    Field& operator=(T v) {
        uint32_t u = static_cast<uint32_t>(v) & kMask;
        uint32_t r = raw();
        r = (r & ~(kMask << Pos)) | (u << Pos);
        raw() = r;
        return *this;
    }

private:
    uint32_t raw() const { return *reinterpret_cast<const uint32_t*>(this); }
    uint32_t& raw() { return *reinterpret_cast<uint32_t*>(this); }
};

} // namespace mgd
} // namespace port
