#pragma once

/// @file mat.hpp
/// @brief Column-major float matrices (OpenGL memory layout) and operations.
///
/// Storage is column-major: element (col, row) lives at m[col * Cols + row].
/// `data()` can be passed directly to glUniformMatrix*fv without conversion.

#include "vec.hpp"
#include <cstddef>

namespace fuhgl::math {

/// @brief 3x3 column-major float matrix (m[col * 3 + row]).
struct Mat3 {
    float m[9] = {1.f, 0.f, 0.f,
                  0.f, 1.f, 0.f,
                  0.f, 0.f, 1.f};

    constexpr Mat3() = default;
    /// @brief Build from 9 values in column-major order.
    constexpr Mat3(float c0r0, float c0r1, float c0r2,
                   float c1r0, float c1r1, float c1r2,
                   float c2r0, float c2r1, float c2r2)
        : m{c0r0, c0r1, c0r2, c1r0, c1r1, c1r2, c2r0, c2r1, c2r2} {}
    /// @brief Build from the upper-left 3x3 block of a Mat4.
    explicit Mat3(const struct Mat4& m4);

    /// @brief Element access: (col, row).
    constexpr float& operator()(int col, int row) { return m[col * 3 + row]; }
    /// @brief Element access: (col, row).
    constexpr const float& operator()(int col, int row) const { return m[col * 3 + row]; }
    /// @brief Column access by index.
    constexpr float* col(int c) { return &m[c * 3]; }
    /// @brief Column access by index.
    constexpr const float* col(int c) const { return &m[c * 3]; }

    /// @brief Raw column-major data, ready for glUniformMatrix3fv.
    constexpr const float* data() const { return m; }
    constexpr float* data() { return m; }

    /// @brief Matrix product.
    constexpr Mat3 operator*(const Mat3& b) const {
        Mat3 r;
        for (int c = 0; c < 3; ++c)
            for (int w = 0; w < 3; ++w) {
                float s = 0.f;
                for (int k = 0; k < 3; ++k) s += (*this)(k, w) * b(c, k);
                r(c, w) = s;
            }
        return r;
    }
    /// @brief Matrix-vector product (vector treated as column).
    constexpr Vec3 operator*(const Vec3& v) const {
        return {
            (*this)(0, 0) * v.x + (*this)(1, 0) * v.y + (*this)(2, 0) * v.z,
            (*this)(0, 1) * v.x + (*this)(1, 1) * v.y + (*this)(2, 1) * v.z,
            (*this)(0, 2) * v.x + (*this)(1, 2) * v.y + (*this)(2, 2) * v.z,
        };
    }

    /// @brief Transpose.
    constexpr Mat3 transpose() const {
        Mat3 r;
        for (int c = 0; c < 3; ++c)
            for (int w = 0; w < 3; ++w) r(c, w) = (*this)(w, c);
        return r;
    }
    /// @brief Determinant.
    constexpr float determinant() const {
        return (*this)(0, 0) * ((*this)(1, 1) * (*this)(2, 2) - (*this)(2, 1) * (*this)(1, 2))
             - (*this)(1, 0) * ((*this)(0, 1) * (*this)(2, 2) - (*this)(2, 1) * (*this)(0, 2))
             + (*this)(2, 0) * ((*this)(0, 1) * (*this)(1, 2) - (*this)(1, 1) * (*this)(0, 2));
    }
    /// @brief Inverse; returns identity if singular.
    Mat3 inverse() const;
};

/// @brief 4x4 column-major float matrix (m[col * 4 + row]). Default is identity.
struct Mat4 {
    float m[16] = {1.f, 0.f, 0.f, 0.f,
                   0.f, 1.f, 0.f, 0.f,
                   0.f, 0.f, 1.f, 0.f,
                   0.f, 0.f, 0.f, 1.f};

    constexpr Mat4() = default;
    /// @brief Build from 16 values in column-major order.
    constexpr Mat4(float c0r0, float c0r1, float c0r2, float c0r3,
                   float c1r0, float c1r1, float c1r2, float c1r3,
                   float c2r0, float c2r1, float c2r2, float c2r3,
                   float c3r0, float c3r1, float c3r2, float c3r3)
        : m{c0r0, c0r1, c0r2, c0r3, c1r0, c1r1, c1r2, c1r3,
            c2r0, c2r1, c2r2, c2r3, c3r0, c3r1, c3r2, c3r3} {}
    /// @brief Build from 4 column vectors.
    constexpr Mat4(const Vec4& c0, const Vec4& c1, const Vec4& c2, const Vec4& c3) {
        const float cols[4][4] = {{c0.x, c0.y, c0.z, c0.w}, {c1.x, c1.y, c1.z, c1.w},
                                  {c2.x, c2.y, c2.z, c2.w}, {c3.x, c3.y, c3.z, c3.w}};
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) m[c * 4 + r] = cols[c][r];
    }

    /// @brief Element access: (col, row).
    constexpr float& operator()(int col, int row) { return m[col * 4 + row]; }
    /// @brief Element access: (col, row).
    constexpr const float& operator()(int col, int row) const { return m[col * 4 + row]; }
    /// @brief Column access by index.
    constexpr Vec4 col(int c) const { return {m[c * 4], m[c * 4 + 1], m[c * 4 + 2], m[c * 4 + 3]}; }

    /// @brief Raw column-major data, ready for glUniformMatrix4fv.
    constexpr const float* data() const { return m; }
    constexpr float* data() { return m; }

    /// @brief Matrix product: (*this) * b.
    constexpr Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int w = 0; w < 4; ++w) {
                float s = 0.f;
                for (int k = 0; k < 4; ++k) s += (*this)(k, w) * b(c, k);
                r(c, w) = s;
            }
        return r;
    }
    /// @brief Matrix-vector product (vector treated as column).
    constexpr Vec4 operator*(const Vec4& v) const {
        return {
            (*this)(0, 0) * v.x + (*this)(1, 0) * v.y + (*this)(2, 0) * v.z + (*this)(3, 0) * v.w,
            (*this)(0, 1) * v.x + (*this)(1, 1) * v.y + (*this)(2, 1) * v.z + (*this)(3, 1) * v.w,
            (*this)(0, 2) * v.x + (*this)(1, 2) * v.y + (*this)(2, 2) * v.z + (*this)(3, 2) * v.w,
            (*this)(0, 3) * v.x + (*this)(1, 3) * v.y + (*this)(2, 3) * v.z + (*this)(3, 3) * v.w,
        };
    }

    /// @brief Transpose.
    constexpr Mat4 transpose() const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int w = 0; w < 4; ++w) r(c, w) = (*this)(w, c);
        return r;
    }
    /// @brief Determinant (expansion along the first column).
    float determinant() const;
    /// @brief General inverse (adjugate method); returns identity if singular.
    Mat4 inverse() const;
};

inline Mat3::Mat3(const Mat4& m4) {
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r) m[c * 3 + r] = m4(c, r);
}

inline Mat3 Mat3::inverse() const {
    // Row-major view: e(r, c) = (*this)(c, r)
    const auto e = [this](int r, int c) { return (*this)(c, r); };
    const float det = determinant();
    if (det == 0.f) return Mat3{};
    const float inv = 1.f / det;
    Mat3 r;
    r(0, 0) = (e(1, 1) * e(2, 2) - e(1, 2) * e(2, 1)) * inv;
    r(1, 0) = -(e(0, 1) * e(2, 2) - e(0, 2) * e(2, 1)) * inv;
    r(2, 0) = (e(0, 1) * e(1, 2) - e(0, 2) * e(1, 1)) * inv;
    r(0, 1) = -(e(1, 0) * e(2, 2) - e(1, 2) * e(2, 0)) * inv;
    r(1, 1) = (e(0, 0) * e(2, 2) - e(0, 2) * e(2, 0)) * inv;
    r(2, 1) = -(e(0, 0) * e(1, 2) - e(0, 2) * e(1, 0)) * inv;
    r(0, 2) = (e(1, 0) * e(2, 1) - e(1, 1) * e(2, 0)) * inv;
    r(1, 2) = -(e(0, 0) * e(2, 1) - e(0, 1) * e(2, 0)) * inv;
    r(2, 2) = (e(0, 0) * e(1, 1) - e(0, 1) * e(1, 0)) * inv;
    return r;
}

inline float Mat4::determinant() const {
    const float* m = this->m;
    const float a00 = m[0], a01 = m[1], a02 = m[2], a03 = m[3];
    const float a10 = m[4], a11 = m[5], a12 = m[6], a13 = m[7];
    const float a20 = m[8], a21 = m[9], a22 = m[10], a23 = m[11];
    const float a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];
    const float b00 = a00 * a11 - a01 * a10;
    const float b01 = a00 * a12 - a02 * a10;
    const float b02 = a00 * a13 - a03 * a10;
    const float b03 = a01 * a12 - a02 * a11;
    const float b04 = a01 * a13 - a03 * a11;
    const float b05 = a02 * a13 - a03 * a12;
    const float b06 = a20 * a31 - a21 * a30;
    const float b07 = a20 * a32 - a22 * a30;
    const float b08 = a20 * a33 - a23 * a30;
    const float b09 = a21 * a32 - a22 * a31;
    const float b10 = a21 * a33 - a23 * a31;
    const float b11 = a22 * a33 - a23 * a32;
    return b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
}

inline Mat4 Mat4::inverse() const {
    const float* m = this->m;
    const float a00 = m[0], a01 = m[1], a02 = m[2], a03 = m[3];
    const float a10 = m[4], a11 = m[5], a12 = m[6], a13 = m[7];
    const float a20 = m[8], a21 = m[9], a22 = m[10], a23 = m[11];
    const float a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];
    const float b00 = a00 * a11 - a01 * a10;
    const float b01 = a00 * a12 - a02 * a10;
    const float b02 = a00 * a13 - a03 * a10;
    const float b03 = a01 * a12 - a02 * a11;
    const float b04 = a01 * a13 - a03 * a11;
    const float b05 = a02 * a13 - a03 * a12;
    const float b06 = a20 * a31 - a21 * a30;
    const float b07 = a20 * a32 - a22 * a30;
    const float b08 = a20 * a33 - a23 * a30;
    const float b09 = a21 * a32 - a22 * a31;
    const float b10 = a21 * a33 - a23 * a31;
    const float b11 = a22 * a33 - a23 * a32;
    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    if (det == 0.f) return Mat4{};
    det = 1.f / det;
    Mat4 r;
    float* o = r.m;
    o[0] = (a11 * b11 - a12 * b10 + a13 * b09) * det;
    o[1] = (a02 * b10 - a01 * b11 - a03 * b09) * det;
    o[2] = (a31 * b05 - a32 * b04 + a33 * b03) * det;
    o[3] = (a22 * b04 - a21 * b05 - a23 * b03) * det;
    o[4] = (a12 * b08 - a10 * b11 - a13 * b07) * det;
    o[5] = (a00 * b11 - a02 * b08 + a03 * b07) * det;
    o[6] = (a32 * b02 - a30 * b05 - a33 * b01) * det;
    o[7] = (a20 * b05 - a22 * b02 + a23 * b01) * det;
    o[8] = (a10 * b10 - a11 * b08 + a13 * b06) * det;
    o[9] = (a01 * b08 - a00 * b10 - a03 * b06) * det;
    o[10] = (a30 * b04 - a31 * b02 + a33 * b00) * det;
    o[11] = (a21 * b02 - a20 * b04 - a23 * b00) * det;
    o[12] = (a11 * b07 - a10 * b09 - a12 * b06) * det;
    o[13] = (a00 * b09 - a01 * b07 + a02 * b06) * det;
    o[14] = (a31 * b01 - a30 * b03 - a32 * b00) * det;
    o[15] = (a20 * b03 - a21 * b01 + a22 * b00) * det;
    return r;
}

} // namespace fuhgl::math
