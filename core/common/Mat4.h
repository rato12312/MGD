#pragma once

#include "Vec3.h"
#include "Vec4.h"
#include <cmath>
#include <optional>
#include <cstring>

namespace mgd {

struct Mat4 {
    float m[16];

    Mat4();

    static Mat4 identity();
    static Mat4 translate(const Vec3& t);
    static Mat4 rotateX(float radians);
    static Mat4 rotateY(float radians);
    static Mat4 rotateZ(float radians);
    static Mat4 rotation(const Vec3& euler);
    static Mat4 scale(const Vec3& s);
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up);
    static Mat4 perspective(float fovRadians, float aspect, float nearPlane, float farPlane);
    static Mat4 ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    static Mat4 multiply(const Mat4& a, const Mat4& b);

    Mat4 operator*(const Mat4& o) const;

    Mat4& operator*=(const Mat4& o);

    Vec4 transformPoint(const Vec4& v) const;
    Vec4 transformDirection(const Vec4& v) const;
    Vec3 transformPoint(const Vec3& v) const;
    Vec3 transformDirection(const Vec3& v) const;

    std::optional<Mat4> inverse() const;
    Mat4 transpose() const;

    const float* data() const { return m; }
    float* data() { return m; }

    static constexpr int ROWS = 4;
    static constexpr int COLS = 4;

private:
    float& at(int row, int col) { return m[col * 4 + row]; }
    const float& at(int row, int col) const { return m[col * 4 + row]; }

    friend struct Vec4;
};

} // namespace mgd
