#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>

using namespace fuhgl::math;

namespace {

bool matNear(const Mat4& a, const Mat4& b, float eps = 1e-5f) {
    for (int i = 0; i < 16; ++i)
        if (std::fabs(a.m[i] - b.m[i]) > eps) return false;
    return true;
}

} // namespace

void runMathTests() {
    // --- vectors ---
    const Vec3 a{1.f, 2.f, 3.f}, b{4.f, 5.f, 6.f};
    CHECK(a + b == Vec3(5.f, 7.f, 9.f));
    CHECK(a - b == Vec3(-3.f, -3.f, -3.f));
    CHECK(a * 2.f == Vec3(2.f, 4.f, 6.f));
    CHECK_NEAR(dot(a, b), 32.f, 1e-6);
    CHECK(cross(Vec3(1, 0, 0), Vec3(0, 1, 0)) == Vec3(0, 0, 1));
    CHECK_NEAR(length(Vec3(3.f, 4.f, 0.f)), 5.f, 1e-6);
    CHECK(normalize(Vec3(0.f, 3.f, 0.f)) == Vec3(0.f, 1.f, 0.f));
    CHECK(lerp(Vec3(0), Vec3(10.f, 0.f, 0.f), 0.5f) == Vec3(5.f, 0.f, 0.f));

    // --- Mat4 basics ---
    const Mat4 I;
    const Mat4 t = translate({1.f, 2.f, 3.f});
    const Vec4 p = t * Vec4(1.f, 1.f, 1.f, 1.f);
    CHECK(p == Vec4(2.f, 3.f, 4.f, 1.f));
    CHECK(matNear(I * t, t));
    CHECK(matNear(t * I, t));

    // rotate 90 deg around Z: (1,0,0) -> (0,1,0)
    const Mat4 rz = rotateZ(kPi * 0.5f);
    const Vec4 rp = rz * Vec4(1.f, 0.f, 0.f, 1.f);
    CHECK_NEAR(rp.x, 0.f, 1e-6);
    CHECK_NEAR(rp.y, 1.f, 1e-6);

    // inverse: M * M^-1 == I
    const Mat4 m = translate({3.f, -2.f, 5.f}) * rotateY(0.7f) * scale({2.f, 3.f, 4.f});
    CHECK(matNear(m * m.inverse(), Mat4{}));
    CHECK_NEAR(m.determinant() * m.inverse().determinant(), 1.f, 1e-4);

    // transpose twice == identity
    CHECK(matNear(m.transpose().transpose(), m));

    // --- perspective: point on near plane center maps to NDC z = -1 ---
    const Mat4 proj = perspective(radians(90.f), 1.f, 1.f, 100.f);
    const Vec4 near_ = proj * Vec4(0.f, 0.f, -1.f, 1.f); // camera looks down -Z
    CHECK_NEAR(near_.z / near_.w, -1.f, 1e-5);
    const Vec4 far_ = proj * Vec4(0.f, 0.f, -100.f, 1.f);
    CHECK_NEAR(far_.z / far_.w, 1.f, 1e-4);
    CHECK_NEAR(proj(0, 0), 1.f, 1e-6); // f/aspect with fov=90, aspect=1 -> f=1

    // --- ortho ---
    const Mat4 o = ortho(-10.f, 10.f, -5.f, 5.f, -1.f, 1.f);
    CHECK_NEAR(o(0, 0), 0.1f, 1e-6);
    CHECK_NEAR(o(1, 1), 0.2f, 1e-6);
    CHECK(o * Vec4(-10.f, -5.f, 0.f, 1.f) == Vec4(-1.f, -1.f, 0.f, 1.f) ||
          [&] { Vec4 v = o * Vec4(-10.f, -5.f, 0.f, 1.f); return std::fabs(v.x + 1) < 1e-6 && std::fabs(v.y + 1) < 1e-6 && std::fabs(v.z) < 1e-6; }());

    // --- lookAt: eye at origin looking down -Z is identity ---
    const Mat4 v = lookAt({0.f, 0.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 1.f, 0.f});
    CHECK(matNear(v, Mat4{}));

    // --- Mat3 from Mat4 ---
    const Mat3 m3(m);
    CHECK_NEAR(m3(0, 0), m(0, 0), 1e-6);
    const Mat3 m3inv = m3.inverse();
    const Mat3 prod = m3 * m3inv;
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r)
            CHECK_NEAR(prod(c, r), c == r ? 1.f : 0.f, 1e-4);

    // --- scalar helpers ---
    CHECK_NEAR(radians(180.f), kPi, 1e-6);
    CHECK_NEAR(degrees(kPi), 180.f, 1e-4);
    CHECK(clamp(5, 0, 3) == 3);
    CHECK(clamp(-5, 0, 3) == 0);
    CHECK_NEAR(smoothstep(0.f, 1.f, 0.5f), 0.5f, 1e-6);
}
