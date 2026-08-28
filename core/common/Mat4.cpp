#include "Mat4.h"
#include <cmath>
#include <cstring>

namespace mgd {

Mat4::Mat4() {
    std::memset(m, 0, sizeof(m));
}

Mat4 Mat4::identity() {
    Mat4 r;
    r.m[0] = 1.0f; r.m[5] = 1.0f; r.m[10] = 1.0f; r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::translate(const Vec3& t) {
    Mat4 r = identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

Mat4 Mat4::rotateX(float rad) {
    float c = std::cos(rad);
    float s = std::sin(rad);
    Mat4 r = identity();
    r.m[5] = c;   r.m[6] = s;
    r.m[9] = -s;  r.m[10] = c;
    return r;
}

Mat4 Mat4::rotateY(float rad) {
    float c = std::cos(rad);
    float s = std::sin(rad);
    Mat4 r = identity();
    r.m[0] = c;   r.m[2] = -s;
    r.m[8] = s;   r.m[10] = c;
    return r;
}

Mat4 Mat4::rotateZ(float rad) {
    float c = std::cos(rad);
    float s = std::sin(rad);
    Mat4 r = identity();
    r.m[0] = c;   r.m[1] = s;
    r.m[4] = -s;  r.m[5] = c;
    return r;
}

Mat4 Mat4::rotation(const Vec3& euler) {
    return rotateZ(euler.z) * rotateX(euler.x) * rotateY(euler.y);
}

Mat4 Mat4::scale(const Vec3& s) {
    Mat4 r;
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& target, const Vec3& worldUp) {
    Vec3 f = (target - eye).normalized();
    Vec3 r = f.cross(worldUp).normalized();
    if (r.lengthSq() < 1e-8f) {
        r = Vec3(1.0f, 0.0f, 0.0f);
    }
    Vec3 u = r.cross(f);

    Mat4 v = identity();
    v.m[0] = r.x;  v.m[4] = r.y;  v.m[8]  = r.z;  v.m[12] = -r.dot(eye);
    v.m[1] = u.x;  v.m[5] = u.y;  v.m[9]  = u.z;  v.m[13] = -u.dot(eye);
    v.m[2] = -f.x; v.m[6] = -f.y; v.m[10] = -f.z; v.m[14] = f.dot(eye);
    v.m[3] = 0.0f; v.m[7] = 0.0f; v.m[11] = 0.0f; v.m[15] = 1.0f;
    return v;
}

Mat4 Mat4::perspective(float fovRadians, float aspect, float nearPlane, float farPlane) {
    float tanHalf = std::tan(fovRadians * 0.5f);
    float rangeInv = 1.0f / (nearPlane - farPlane);

    Mat4 r;
    r.m[0] = 1.0f / (aspect * tanHalf);
    r.m[5] = 1.0f / tanHalf;
    r.m[10] = (farPlane) * rangeInv;
    r.m[11] = -1.0f;
    r.m[14] = (nearPlane * farPlane) * rangeInv;
    return r;
}

Mat4 Mat4::ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    float rl = 1.0f / (right - left);
    float tb = 1.0f / (top - bottom);
    float fn = 1.0f / (farPlane - nearPlane);

    Mat4 r;
    r.m[0] = 2.0f * rl;
    r.m[5] = 2.0f * tb;
    r.m[10] = -2.0f * fn;
    r.m[12] = -(right + left) * rl;
    r.m[13] = -(top + bottom) * tb;
    r.m[14] = -(farPlane + nearPlane) * fn;
    r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[c * 4 + k];
            }
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 Mat4::operator*(const Mat4& o) const {
    return multiply(*this, o);
}

Mat4& Mat4::operator*=(const Mat4& o) {
    *this = multiply(*this, o);
    return *this;
}

Vec4 Mat4::transformPoint(const Vec4& v) const {
    return {
        m[0]*v.x + m[4]*v.y + m[8]*v.z  + m[12]*v.w,
        m[1]*v.x + m[5]*v.y + m[9]*v.z  + m[13]*v.w,
        m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
        m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
    };
}

Vec4 Mat4::transformDirection(const Vec4& v) const {
    return {
        m[0]*v.x + m[4]*v.y + m[8]*v.z,
        m[1]*v.x + m[5]*v.y + m[9]*v.z,
        m[2]*v.x + m[6]*v.y + m[10]*v.z,
        0.0f
    };
}

Vec3 Mat4::transformPoint(const Vec3& v) const {
    Vec4 r = transformPoint(Vec4(v, 1.0f));
    if (std::abs(r.w) > 1e-8f) {
        return {r.x / r.w, r.y / r.w, r.z / r.w};
    }
    return {r.x, r.y, r.z};
}

Vec3 Mat4::transformDirection(const Vec3& v) const {
    Vec4 r = transformDirection(Vec4(v, 0.0f));
    return {r.x, r.y, r.z};
}

std::optional<Mat4> Mat4::inverse() const {
    float inv[16];
    float det;

    inv[0]  = m[5]  * m[10] * m[15] - m[5]  * m[11] * m[14] - m[9]  * m[6]  * m[15] + m[9]  * m[7]  * m[14] + m[13] * m[6]  * m[11] - m[13] * m[7]  * m[10];
    inv[4]  = -m[4] * m[10] * m[15] + m[4]  * m[11] * m[14] + m[8]  * m[6]  * m[15] - m[8]  * m[7]  * m[14] - m[12] * m[6]  * m[11] + m[12] * m[7]  * m[10];
    inv[8]  = m[4]  * m[9]  * m[15] - m[4]  * m[11] * m[13] - m[8]  * m[5]  * m[15] + m[8]  * m[7]  * m[13] + m[12] * m[5]  * m[11] - m[12] * m[7]  * m[9];
    inv[12] = -m[4] * m[9]  * m[14] + m[4]  * m[10] * m[13] + m[8]  * m[5]  * m[14] - m[8]  * m[6]  * m[13] - m[12] * m[5]  * m[10] + m[12] * m[6]  * m[9];
    inv[1]  = -m[1] * m[10] * m[15] + m[1]  * m[11] * m[14] + m[9]  * m[2]  * m[15] - m[9]  * m[3]  * m[14] - m[13] * m[2]  * m[11] + m[13] * m[3]  * m[10];
    inv[5]  = m[0]  * m[10] * m[15] - m[0]  * m[11] * m[14] - m[8]  * m[2]  * m[15] + m[8]  * m[3]  * m[14] + m[12] * m[2]  * m[11] - m[12] * m[3]  * m[10];
    inv[9]  = -m[0] * m[9]  * m[15] + m[0]  * m[11] * m[13] + m[8]  * m[1]  * m[15] - m[8]  * m[3]  * m[13] - m[12] * m[1]  * m[11] + m[12] * m[3]  * m[9];
    inv[13] = m[0]  * m[9]  * m[14] - m[0]  * m[10] * m[13] - m[8]  * m[1]  * m[14] + m[8]  * m[2]  * m[13] + m[12] * m[1]  * m[10] - m[12] * m[2]  * m[9];
    inv[2]  = m[1]  * m[6]  * m[15] - m[1]  * m[7]  * m[14] - m[5]  * m[2]  * m[15] + m[5]  * m[3]  * m[14] + m[13] * m[2]  * m[7]  - m[13] * m[3]  * m[6];
    inv[6]  = -m[0] * m[6]  * m[15] + m[0]  * m[7]  * m[14] + m[4]  * m[2]  * m[15] - m[4]  * m[3]  * m[14] - m[12] * m[2]  * m[7]  + m[12] * m[3]  * m[6];
    inv[10] = m[0]  * m[5]  * m[15] - m[0]  * m[7]  * m[13] - m[4]  * m[1]  * m[15] + m[4]  * m[3]  * m[13] + m[12] * m[1]  * m[7]  - m[12] * m[3]  * m[5];
    inv[14] = -m[0] * m[5]  * m[14] + m[0]  * m[6]  * m[13] + m[4]  * m[1]  * m[14] - m[4]  * m[2]  * m[13] - m[12] * m[1]  * m[6]  + m[12] * m[2]  * m[5];
    inv[3]  = -m[1] * m[6]  * m[11] + m[1]  * m[7]  * m[10] + m[5]  * m[2]  * m[11] - m[5]  * m[3]  * m[10] - m[9]  * m[2]  * m[7]  + m[9]  * m[3]  * m[6];
    inv[7]  = m[0]  * m[6]  * m[11] - m[0]  * m[7]  * m[10] - m[4]  * m[2]  * m[11] + m[4]  * m[3]  * m[10] + m[8]  * m[2]  * m[7]  - m[8]  * m[3]  * m[6];
    inv[11] = -m[0] * m[5]  * m[11] + m[0]  * m[7]  * m[9]  + m[4]  * m[1]  * m[11] - m[4]  * m[3]  * m[9]  - m[8]  * m[1]  * m[7]  + m[8]  * m[3]  * m[5];
    inv[15] = m[0]  * m[5]  * m[10] - m[0]  * m[6]  * m[9]  - m[4]  * m[1]  * m[10] + m[4]  * m[2]  * m[9]  + m[8]  * m[1]  * m[6]  - m[8]  * m[2]  * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (std::abs(det) < 1e-8f) {
        return std::nullopt;
    }

    det = 1.0f / det;

    Mat4 r;
    for (int i = 0; i < 16; ++i) {
        r.m[i] = inv[i] * det;
    }
    return r;
}

Mat4 Mat4::transpose() const {
    Mat4 r;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i * 4 + j] = m[j * 4 + i];
        }
    }
    return r;
}

} // namespace mgd
