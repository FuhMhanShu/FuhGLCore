#pragma once

/// @file func.hpp
/// @brief Scalar helpers and 3D transform builders (right-handed, OpenGL NDC z in [-1, 1]).

#include "mat.hpp"
#include "vec.hpp"
#include <cmath>

namespace fuhgl::math {

/// @brief Pi as float.
inline constexpr float kPi = 3.14159265358979323846f;

/// @brief Degrees -> radians.
constexpr float radians(float degrees) { return degrees * (kPi / 180.f); }
/// @brief Radians -> degrees.
constexpr float degrees(float radians_) { return radians_ * (180.f / kPi); }

/// @brief Clamp to [lo, hi].
template <typename T>
constexpr T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
/// @brief Smooth Hermite interpolation between edge0 and edge1.
inline float smoothstep(float edge0, float edge1, float x) {
    const float t = clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// --- transform builders -------------------------------------------------------

/// @brief Translation matrix.
inline Mat4 translate(const Vec3& t) {
    Mat4 r;
    r(3, 0) = t.x; r(3, 1) = t.y; r(3, 2) = t.z;
    return r;
}
/// @brief Scale matrix.
inline Mat4 scale(const Vec3& s) {
    Mat4 r;
    r(0, 0) = s.x; r(1, 1) = s.y; r(2, 2) = s.z;
    return r;
}
/// @brief Rotation matrix from axis (need not be normalized) and angle in radians (Rodrigues).
inline Mat4 rotate(float angleRad, const Vec3& axis) {
    const Vec3 a = normalize(axis);
    const float c = std::cos(angleRad), s = std::sin(angleRad), t = 1.f - c;
    Mat4 r;
    r(0, 0) = t * a.x * a.x + c;       r(0, 1) = t * a.x * a.y + s * a.z; r(0, 2) = t * a.x * a.z - s * a.y;
    r(1, 0) = t * a.x * a.y - s * a.z; r(1, 1) = t * a.y * a.y + c;     r(1, 2) = t * a.y * a.z + s * a.x;
    r(2, 0) = t * a.x * a.z + s * a.y; r(2, 1) = t * a.y * a.z - s * a.x; r(2, 2) = t * a.z * a.z + c;
    return r;
}
/// @brief Rotation around the X axis, angle in radians.
inline Mat4 rotateX(float a) { return rotate(a, {1.f, 0.f, 0.f}); }
/// @brief Rotation around the Y axis, angle in radians.
inline Mat4 rotateY(float a) { return rotate(a, {0.f, 1.f, 0.f}); }
/// @brief Rotation around the Z axis, angle in radians.
inline Mat4 rotateZ(float a) { return rotate(a, {0.f, 0.f, 1.f}); }

/// @brief Right-handed perspective projection (GL NDC: z in [-1, 1], camera looks down -Z).
/// @param fovYRadians vertical field of view in radians
/// @param aspect width / height
/// @param zNear near plane distance (> 0)
/// @param zFar far plane distance (> zNear)
inline Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
    const float f = 1.f / std::tan(fovYRadians * 0.5f);
    Mat4 r{}; // zero
    for (float& v : r.m) v = 0.f;
    r(0, 0) = f / aspect;
    r(1, 1) = f;
    r(2, 2) = (zFar + zNear) / (zNear - zFar);
    r(2, 3) = -1.f;
    r(3, 2) = 2.f * zFar * zNear / (zNear - zFar);
    return r;
}

/// @brief Right-handed orthographic projection (GL NDC: z in [-1, 1]).
inline Mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
    Mat4 r;
    r(0, 0) = 2.f / (right - left);
    r(1, 1) = 2.f / (top - bottom);
    r(2, 2) = -2.f / (zFar - zNear);
    r(3, 0) = -(right + left) / (right - left);
    r(3, 1) = -(top + bottom) / (top - bottom);
    r(3, 2) = -(zFar + zNear) / (zFar - zNear);
    return r;
}

/// @brief Right-handed view matrix looking from eye towards center with the given up vector.
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r;
    r(0, 0) = s.x;  r(1, 0) = s.y;  r(2, 0) = s.z;   // row 0 = s
    r(0, 1) = u.x;  r(1, 1) = u.y;  r(2, 1) = u.z;   // row 1 = u
    r(0, 2) = -f.x; r(1, 2) = -f.y; r(2, 2) = -f.z;  // row 2 = -f
    r(3, 0) = -dot(s, eye);
    r(3, 1) = -dot(u, eye);
    r(3, 2) = dot(f, eye);
    return r;
}

} // namespace fuhgl::math
