#pragma once

#include <cstdint>

namespace mgd {

struct RGBA {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    constexpr RGBA() = default;
    constexpr RGBA(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    constexpr bool operator==(const RGBA& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    constexpr bool operator!=(const RGBA& o) const { return !(*this == o); }
};

} // namespace mgd
