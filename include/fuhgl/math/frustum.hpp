#pragma once

/// @file frustum.hpp
/// @brief View frustum extraction (Gribb/Hartmann) and screen-space picking rays.

#include "mat.hpp"
#include "vec.hpp"

namespace fuhgl::math {

/// @brief Plane in Hessian normal form: n . p + d = 0, |n| == 1.
struct Plane {
    Vec3 n;
    float d;
};

/// @brief View frustum as 6 inward-facing planes (left, right, bottom, top, near, far).
struct Frustum {
    Plane planes[6];

    /// @brief Extract the frustum from a combined view-projection matrix.
    static Frustum fromViewProj(const Mat4& vp) {
        // Row access for column-major storage: M(r, c) = vp(c, r)
        const auto M = [&vp](int r, int c) { return vp(c, r); };
        Frustum f;
        float rows[4][4];
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) rows[r][c] = M(r, c);
        // Gribb/Hartmann: plane = row3 +/- row_r, normalized.
        auto plane = [&](int r, float sign) {
            const float a = rows[3][0] + sign * rows[r][0];
            const float b = rows[3][1] + sign * rows[r][1];
            const float c = rows[3][2] + sign * rows[r][2];
            const float d = rows[3][3] + sign * rows[r][3];
            const float len = std::sqrt(a * a + b * b + c * c);
            const float inv = len > 0.f ? 1.f / len : 0.f;
            return Plane{{a * inv, b * inv, c * inv}, d * inv};
        };
        f.planes[0] = plane(0, +1.f); // left   = row3 + row0
        f.planes[1] = plane(0, -1.f); // right  = row3 - row0
        f.planes[2] = plane(1, +1.f); // bottom = row3 + row1
        f.planes[3] = plane(1, -1.f); // top    = row3 - row1
        f.planes[4] = plane(2, +1.f); // near   = row3 + row2
        f.planes[5] = plane(2, -1.f); // far    = row3 - row2
        return f;
    }

    /// @brief Point-in-frustum test (all planes: n . p + d >= 0).
    bool containsPoint(const Vec3& p) const {
        for (const Plane& pl : planes)
            if (dot(pl.n, p) + pl.d < 0.f) return false;
        return true;
    }
    /// @brief Sphere-in-frustum test (conservative).
    bool containsSphere(const Vec3& center, float radius) const {
        for (const Plane& pl : planes)
            if (dot(pl.n, center) + pl.d < -radius) return false;
        return true;
    }
    /// @brief AABB/frustum intersection test (positive-vertex test per plane).
    bool intersectsAabb(const Vec3& mn, const Vec3& mx) const {
        for (const Plane& pl : planes) {
            const Vec3 pv{pl.n.x >= 0.f ? mx.x : mn.x,
                          pl.n.y >= 0.f ? mx.y : mn.y,
                          pl.n.z >= 0.f ? mx.z : mn.z};
            if (dot(pl.n, pv) + pl.d < 0.f) return false;
        }
        return true;
    }
};

/// @brief Transforms a bounding sphere (xyz = center, w = radius, object space) into
/// world space by a model matrix: the center is transformed by the matrix, the radius
/// is scaled by the largest axis scale (exact for rotations/translations/uniform scale,
/// conservative for non-uniform scale) — used by GPU/CPU frustum culling (F34).
inline Vec4 transformSphere(const Mat4& model, const Vec4& localSphere) {
    const Vec4 c = model * Vec4(localSphere.x, localSphere.y, localSphere.z, 1.f);
    const float sx = length(Vec3(model.col(0).x, model.col(0).y, model.col(0).z));
    const float sy = length(Vec3(model.col(1).x, model.col(1).y, model.col(1).z));
    const float sz = length(Vec3(model.col(2).x, model.col(2).y, model.col(2).z));
    const float s = sx > sy ? (sx > sz ? sx : sz) : (sy > sz ? sy : sz);
    return {c.x, c.y, c.z, localSphere.w * s};
}

/// @brief Ray in world space.
struct Ray {
    Vec3 origin;
    Vec3 dir; ///< normalized
};

/// @brief World-space ray through a screen pixel (x, y in pixels, y down from top).
inline Ray rayFromScreen(const Mat4& viewProj, float x, float y, float width, float height) {
    const float ndcX = 2.f * x / width - 1.f;
    const float ndcY = 1.f - 2.f * y / height;
    const Mat4 inv = viewProj.inverse();
    const Vec4 pn = inv * Vec4(ndcX, ndcY, -1.f, 1.f);
    const Vec4 pf = inv * Vec4(ndcX, ndcY, 1.f, 1.f);
    const Vec3 p0{pn.x / pn.w, pn.y / pn.w, pn.z / pn.w};
    const Vec3 p1{pf.x / pf.w, pf.y / pf.w, pf.z / pf.w};
    return {p0, normalize(p1 - p0)};
}

} // namespace fuhgl::math
