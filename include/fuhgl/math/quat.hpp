#pragma once

/// @file quat.hpp
/// @brief Unit quaternions for rotations (Hamilton convention, right-handed).
///
/// A Quat (x, y, z, w) rotates a vector via v' = q * v * q^-1 (see operator*),
/// which is exactly the rotation of math::rotate(angle, axis) — toMat4(q)
/// equals rotate(angle, axis) for q = fromAxisAngle(angle, axis).

#include "mat.hpp"
#include "vec.hpp"

#include <cmath>

namespace fuhgl::math {

/// @brief Quaternion (x, y, z, w); the default constructed value is identity (0, 0, 0, 1).
struct Quat {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;

    constexpr Quat() = default;
    constexpr Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    /// @brief Hamilton product (composition: `a * b` applies b first, then a).
    constexpr Quat operator*(const Quat& q) const {
        return {w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w,
                w * q.w - x * q.x - y * q.y - z * q.z};
    }
    /// @brief Rotates a vector: v' = q * v * q^-1 (q should be unit; see normalize()).
    constexpr Vec3 operator*(const Vec3& v) const {
        const Vec3 u{x, y, z};
        const Vec3 t = cross(u, v) * 2.f;
        return v + t * w + cross(u, t);
    }
    constexpr Quat operator-() const { return {-x, -y, -z, -w}; }
    constexpr Quat operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr bool operator==(const Quat& o) const {
        return x == o.x && y == o.y && z == o.z && w == o.w;
    }
    constexpr bool operator!=(const Quat& o) const { return !(*this == o); }
};

/// @brief Dot product.
constexpr float dot(const Quat& a, const Quat& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
/// @brief Squared length.
constexpr float lengthSq(const Quat& q) { return dot(q, q); }
/// @brief Euclidean length.
inline float length(const Quat& q) { return std::sqrt(lengthSq(q)); }
/// @brief Returns a unit copy; returns identity for zero input.
inline Quat normalize(const Quat& q) {
    const float l = length(q);
    return l > 0.f ? Quat{q.x / l, q.y / l, q.z / l, q.w / l} : Quat{};
}

/// @brief Conjugate {-x, -y, -z, w}; for unit quats this is the inverse rotation.
constexpr Quat conjugate(const Quat& q) { return {-q.x, -q.y, -q.z, q.w}; }
/// @brief Inverse rotation (conjugate / lengthSq); identity for zero input.
inline Quat inverse(const Quat& q) {
    const float l2 = lengthSq(q);
    return l2 > 0.f ? conjugate(q) * (1.f / l2) : Quat{};
}

/// @brief Rotation of `angleRad` around `axis` (need not be normalized).
inline Quat fromAxisAngle(float angleRad, const Vec3& axis) {
    const Vec3 a = normalize(axis);
    const float h = angleRad * 0.5f;
    const float s = std::sin(h);
    return {a.x * s, a.y * s, a.z * s, std::cos(h)};
}

/// @brief Euler angles in radians: yaw around Y, pitch around X, roll around Z.
/// Composition qYaw * qPitch * qRoll — roll is applied first, then pitch, then yaw.
inline Quat fromEuler(float yaw, float pitch, float roll) {
    return fromAxisAngle(yaw, {0.f, 1.f, 0.f}) * fromAxisAngle(pitch, {1.f, 0.f, 0.f}) *
           fromAxisAngle(roll, {0.f, 0.f, 1.f});
}

/// @brief Unit quat from a rotation matrix (upper-left 3x3; trace-based, branchless-ish).
inline Quat fromMat3(const Mat3& m) {
    const float tr = m(0, 0) + m(1, 1) + m(2, 2);
    if (tr > 0.f) {
        const float s = std::sqrt(tr + 1.f) * 2.f;
        return normalize(Quat{(m(1, 2) - m(2, 1)) / s, (m(2, 0) - m(0, 2)) / s,
                               (m(0, 1) - m(1, 0)) / s, s * 0.25f});
    }
    if (m(0, 0) > m(1, 1) && m(0, 0) > m(2, 2)) {
        const float s = std::sqrt(1.f + m(0, 0) - m(1, 1) - m(2, 2)) * 2.f;
        return normalize(Quat{s * 0.25f, (m(1, 0) + m(0, 1)) / s, (m(2, 0) + m(0, 2)) / s,
                               (m(1, 2) - m(2, 1)) / s});
    }
    if (m(1, 1) > m(2, 2)) {
        const float s = std::sqrt(1.f + m(1, 1) - m(0, 0) - m(2, 2)) * 2.f;
        return normalize(Quat{(m(1, 0) + m(0, 1)) / s, s * 0.25f, (m(2, 1) + m(1, 2)) / s,
                               (m(2, 0) - m(0, 2)) / s});
    }
    const float s = std::sqrt(1.f + m(2, 2) - m(0, 0) - m(1, 1)) * 2.f;
    return normalize(Quat{(m(2, 0) + m(0, 2)) / s, (m(2, 1) + m(1, 2)) / s, s * 0.25f,
                           (m(0, 1) - m(1, 0)) / s});
}

/// @brief Rotation matrix of a unit quat (same layout as rotate(angle, axis)).
inline Mat3 toMat3(const Quat& q) {
    const Quat u = normalize(q);
    const float xx = u.x * u.x, yy = u.y * u.y, zz = u.z * u.z;
    const float xy = u.x * u.y, xz = u.x * u.z, yz = u.y * u.z;
    const float wx = u.w * u.x, wy = u.w * u.y, wz = u.w * u.z;
    Mat3 r;
    r(0, 0) = 1.f - 2.f * (yy + zz);
    r(1, 0) = 2.f * (xy - wz);
    r(2, 0) = 2.f * (xz + wy);
    r(0, 1) = 2.f * (xy + wz);
    r(1, 1) = 1.f - 2.f * (xx + zz);
    r(2, 1) = 2.f * (yz - wx);
    r(0, 2) = 2.f * (xz - wy);
    r(1, 2) = 2.f * (yz + wx);
    r(2, 2) = 1.f - 2.f * (xx + yy);
    return r;
}

/// @brief 4x4 rotation matrix of a unit quat (upper-left 3x3 = toMat3, rest identity).
inline Mat4 toMat4(const Quat& q) {
    const Mat3 m = toMat3(q);
    Mat4 r;
    for (int c = 0; c < 3; ++c)
        for (int w = 0; w < 3; ++w) r(c, w) = m(c, w);
    return r;
}

/// @brief Orientation whose local -Z axis points along `forward` (right-handed, `up` hint).
/// lookRotation({0,0,-1}, {0,1,0}) is identity.
inline Quat lookRotation(const Vec3& forward, const Vec3& up) {
    const Vec3 f = normalize(forward);
    const Vec3 rgt = normalize(cross(f, up));
    const Vec3 u = cross(rgt, f);
    Mat3 m;
    for (int i = 0; i < 3; ++i) {
        m(0, i) = rgt[i];
        m(1, i) = u[i];
        m(2, i) = -f[i];
    }
    return fromMat3(m);
}

/// @brief Spherical linear interpolation between unit quats a and b (shortest path).
inline Quat slerp(const Quat& a, const Quat& b, float t) {
    float d = dot(a, b);
    Quat bb = b;
    if (d < 0.f) {
        bb = -bb;
        d = -d;
    }
    if (d > 0.9995f) // nearly equal: linear fallback to avoid division by ~0
        return normalize(Quat{a.x + (bb.x - a.x) * t, a.y + (bb.y - a.y) * t,
                              a.z + (bb.z - a.z) * t, a.w + (bb.w - a.w) * t});
    const float theta = std::acos(d < 1.f ? d : 1.f);
    const float sinT = std::sin(theta);
    const float wa = std::sin((1.f - t) * theta) / sinT;
    const float wb = std::sin(t * theta) / sinT;
    return {a.x * wa + bb.x * wb, a.y * wa + bb.y * wb, a.z * wa + bb.z * wb, a.w * wa + bb.w * wb};
}

/// @brief Normalized linear interpolation (cheap approximation of slerp).
inline Quat nlerp(const Quat& a, const Quat& b, float t) {
    Quat r{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
           a.w + (b.w - a.w) * t};
    if (dot(a, b) < 0.f) r = -r;
    return normalize(r);
}

/// @brief Rotation angle between two quats in radians: 2 * acos(|dot|), in [0, pi].
inline float angle(const Quat& a, const Quat& b) {
    const float d = std::fabs(dot(normalize(a), normalize(b)));
    return 2.f * std::acos(d > 1.f ? 1.f : d);
}

/// @brief Extracts axis and angle (radians) of a unit quat: q = fromAxisAngle(angle, axis).
inline void toAxisAngle(const Quat& q, Vec3& axis, float& angleRad) {
    const Quat u = normalize(q);
    angleRad = 2.f * std::acos(u.w > 1.f ? 1.f : (u.w < -1.f ? -1.f : u.w));
    const float s = std::sin(angleRad * 0.5f);
    axis = s > 1e-6f ? Vec3{u.x / s, u.y / s, u.z / s} : Vec3{1.f, 0.f, 0.f};
}

} // namespace fuhgl::math
