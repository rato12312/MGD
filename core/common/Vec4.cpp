#include "Vec4.h"
#include "Vec3.h"
#include <cmath>

namespace mgd {

constexpr Vec4::Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}

Vec3 Vec4::xyz() const { return {x, y, z}; }

Vec4 Vec4::perspectiveDivide() const {
    if (std::abs(w) < 1e-8f) return {0.0f, 0.0f, 0.0f, 0.0f};
    return {x / w, y / w, z / w, 1.0f};
}

} // namespace mgd
