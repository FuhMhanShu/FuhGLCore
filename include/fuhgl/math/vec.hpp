#pragma once

/// @file vec.hpp
/// @brief Compact constexpr vector types (float) for 2/3/4 components.

#include <cmath>

namespace fuhgl::math {

/// @brief 2-component float vector.
struct Vec2 {
    float x = 0.f, y = 0.f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    constexpr explicit Vec2(float s) : x(s), y(s) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    constexpr bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Vec2& o) const { return !(*this == o); }
};

constexpr Vec2 operator*(float s, const Vec2& v) { return v * s; }

/// @brief 3-component float vector.
struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    constexpr explicit Vec3(float s) : x(s), y(s), z(s) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    constexpr Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    constexpr bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    constexpr bool operator!=(const Vec3& o) const { return !(*this == o); }

    /// @brief Element access by index (0=x, 1=y, 2=z).
    constexpr float& operator[](int i) { return (&x)[i]; }
    constexpr const float& operator[](int i) const { return (&x)[i]; }
};

constexpr Vec3 operator*(float s, const Vec3& v) { return v * s; }

/// @brief 4-component float vector.
struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;

    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    constexpr explicit Vec4(float s) : x(s), y(s), z(s), w(s) {}

    constexpr Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    constexpr Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    constexpr Vec4 operator-() const { return {-x, -y, -z, -w}; }
    constexpr Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr Vec4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    constexpr bool operator==(const Vec4& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    constexpr bool operator!=(const Vec4& o) const { return !(*this == o); }
};

constexpr Vec4 operator*(float s, const Vec4& v) { return v * s; }

// --- vector functions --------------------------------------------------------

/// @brief Dot product.
constexpr float dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
/// @brief Dot product.
constexpr float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
/// @brief Dot product.
constexpr float dot(const Vec4& a, const Vec4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

/// @brief Cross product (right-handed).
constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/// @brief Squared length (cheaper than length()).
constexpr float lengthSq(const Vec2& v) { return dot(v, v); }
/// @brief Squared length (cheaper than length()).
constexpr float lengthSq(const Vec3& v) { return dot(v, v); }
/// @brief Squared length (cheaper than length()).
constexpr float lengthSq(const Vec4& v) { return dot(v, v); }

/// @brief Euclidean length.
inline float length(const Vec2& v) { return std::sqrt(lengthSq(v)); }
/// @brief Euclidean length.
inline float length(const Vec3& v) { return std::sqrt(lengthSq(v)); }
/// @brief Euclidean length.
inline float length(const Vec4& v) { return std::sqrt(lengthSq(v)); }

/// @brief Returns a normalized copy; returns zero vector for zero input.
inline Vec2 normalize(const Vec2& v) { const float l = length(v); return l > 0.f ? v / l : Vec2{}; }
/// @brief Returns a normalized copy; returns zero vector for zero input.
inline Vec3 normalize(const Vec3& v) { const float l = length(v); return l > 0.f ? v / l : Vec3{}; }
/// @brief Returns a normalized copy; returns zero vector for zero input.
inline Vec4 normalize(const Vec4& v) { const float l = length(v); return l > 0.f ? v / l : Vec4{}; }

/// @brief Component-wise linear interpolation: a + (b - a) * t.
constexpr Vec2 lerp(const Vec2& a, const Vec2& b, float t) { return a + (b - a) * t; }
/// @brief Component-wise linear interpolation: a + (b - a) * t.
constexpr Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
/// @brief Component-wise linear interpolation: a + (b - a) * t.
constexpr Vec4 lerp(const Vec4& a, const Vec4& b, float t) { return a + (b - a) * t; }

} // namespace fuhgl::math
