#include "Vec4.h"
#include "Vec3.h"
#include <cmath>

namespace mgd {

Vec3 Vec4::xyz() const { return {x, y, z}; }

Vec4 Vec4::perspectiveDivide() const {
    if (std::abs(w) < 1e-8f) return {0.0f, 0.0f, 0.0f, 0.0f};
    return {x / w, y / w, z / w, 1.0f};
}

} // namespace mgd
