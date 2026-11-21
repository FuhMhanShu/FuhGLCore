#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>

#include <cmath>
#include <initializer_list>

using namespace fuhgl::math;

static bool vecNear(const Vec3& a, const Vec3& b, float eps = 1e-5f) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

static bool matNear(const Mat4& a, const Mat4& b, float eps = 1e-5f) {
    for (int i = 0; i < 16; ++i)
        if (std::fabs(a.data()[i] - b.data()[i]) > eps) return false;
    return true;
}

void runQuatTests() {
    // identity rotates nothing
    const Quat id;
    CHECK(vecNear(id * Vec3{1.f, 2.f, 3.f}, {1.f, 2.f, 3.f}));
    CHECK(matNear(toMat4(id), Mat4{}));

    // 90 deg around Y maps +X to -Z (right-handed), same as rotate(kPi/2, Y)
    const Quat qy = fromAxisAngle(kPi * 0.5f, {0.f, 1.f, 0.f});
    CHECK(vecNear(qy * Vec3{1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}));
    CHECK(vecNear(qy * Vec3{0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}));

    // toMat4(q) == rotate(angle, axis)
    for (float ang : {0.3f, 1.2f, -2.1f, kPi}) {
        const Vec3 axis = normalize(Vec3{1.f, 2.f, 3.f});
        const Quat q = fromAxisAngle(ang, axis);
        CHECK(matNear(toMat4(q), rotate(ang, axis), 1e-4f));
    }

    // composition: (a*b)*v == a*(b*v) and toMat4(a*b) == toMat4(a)*toMat4(b)
    const Quat qa = fromAxisAngle(0.7f, {1.f, 0.f, 0.f});
    const Quat qb = fromAxisAngle(-1.1f, {0.f, 1.f, 0.f});
    const Vec3 v{0.3f, -0.8f, 0.5f};
    CHECK(vecNear((qa * qb) * v, qa * (qb * v)));
    CHECK(matNear(toMat4(qa * qb), toMat4(qa) * toMat4(qb), 1e-4f));

    // inverse undoes the rotation; q * inverse(q) = identity
    const Quat inv = inverse(qa);
    CHECK(vecNear(inv * (qa * v), v, 1e-4f));
    const Quat prod = qa * inv;
    CHECK_NEAR(prod.x, 0.f, 1e-5f);
    CHECK_NEAR(prod.y, 0.f, 1e-5f);
    CHECK_NEAR(prod.z, 0.f, 1e-5f);
    CHECK_NEAR(prod.w, 1.f, 1e-5f);

    // double cover: q and -q rotate identically
    CHECK(vecNear((-qa) * v, qa * v, 1e-5f));

    // normalize / length
    const Quat scaled{0.f, 0.f, 3.f, 4.f};
    CHECK_NEAR(length(scaled), 5.f, 1e-5f);
    CHECK_NEAR(length(normalize(scaled)), 1.f, 1e-5f);

    // fromEuler(0,0,0) = identity; fromEuler(yaw,0,0) == fromAxisAngle(yaw, Y)
    CHECK(fromEuler(0.f, 0.f, 0.f) == Quat{});
    const Quat qe = fromEuler(0.9f, 0.f, 0.f);
    const Quat qy2 = fromAxisAngle(0.9f, {0.f, 1.f, 0.f});
    CHECK_NEAR(qe.x, qy2.x, 1e-6f);
    CHECK_NEAR(qe.w, qy2.w, 1e-6f);

    // lookRotation: default forward = identity, +X forward rotates -Z to +X
    CHECK(vecNear(lookRotation({0.f, 0.f, -1.f}, {0.f, 1.f, 0.f}) * Vec3{0.f, 0.f, -1.f},
                  {0.f, 0.f, -1.f}));
    const Quat qx = lookRotation({1.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
    CHECK(vecNear(qx * Vec3{0.f, 0.f, -1.f}, {1.f, 0.f, 0.f}));

    // slerp endpoints and midpoint angle
    const Quat s0 = slerp(qa, qb, 0.f);
    const Quat s1 = slerp(qa, qb, 1.f);
    CHECK_NEAR(angle(s0, qa), 0.f, 1e-4f);
    CHECK_NEAR(angle(s1, qb), 0.f, 1e-4f);
    const float half = angle(qa, qb) * 0.5f;
    CHECK_NEAR(angle(slerp(qa, qb, 0.5f), qa), half, 1e-3f);

    // nlerp endpoints
    CHECK_NEAR(angle(nlerp(qa, qb, 0.f), qa), 0.f, 1e-4f);
    CHECK_NEAR(angle(nlerp(qa, qb, 1.f), qb), 0.f, 1e-4f);

    // toAxisAngle round-trip
    Vec3 axis;
    float ang = 0.f;
    toAxisAngle(qy, axis, ang);
    CHECK_NEAR(ang, kPi * 0.5f, 1e-4f);
    CHECK(vecNear(axis, {0.f, 1.f, 0.f}, 1e-4f));

    // fromMat3(toMat3(q)) round-trip (up to sign)
    const Quat rt = fromMat3(toMat3(qa));
    CHECK_NEAR(std::fabs(dot(normalize(rt), normalize(qa))), 1.f, 1e-4f);
}
