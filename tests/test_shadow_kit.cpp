#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/render/shadow.hpp>
#include <fuhgl/render/shadow_kit.hpp>

#include <cmath>

using namespace fuhgl;
using namespace fuhgl::render;

namespace {
math::Vec3 projectNdc(const math::Mat4& m, const math::Vec3& p) {
    const math::Vec4 c = m * math::Vec4(p, 1.f);
    return {c.x / c.w, c.y / c.w, c.z / c.w};
}
bool finite(const math::Vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
} // namespace

void runShadowKitTests() {
    // --- pointLightFaceVPs: 6 finite VPs, each face axis maps to the map center -------
    const math::Vec3 pos{2.f, 3.f, 2.f};
    const auto vps = pointLightFaceVPs(pos, 0.5f, 50.f);
    const math::Vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int f = 0; f < 6; ++f) {
        const math::Vec3 c = projectNdc(vps[f], pos + dirs[f] * 10.f);
        CHECK(finite(c));
        CHECK(std::fabs(c.x) < 1e-3f && std::fabs(c.y) < 1e-3f); // face center -> NDC center
        CHECK(c.z > -1.f && c.z < 1.f);
        // a point 45+ degrees off the face axis at depth 5 is outside (half-width at
        // depth 5 is 5; 7.5 off-axis -> ndc 1.5); the point is in front (w > 0)
        const math::Vec3 perp = dirs[f].x != 0.f ? math::Vec3{0.f, 1.f, 0.f} : math::Vec3{1.f, 0.f, 0.f};
        const math::Vec4 cw = vps[f] * math::Vec4(pos + dirs[f] * 5.f + perp * 7.5f, 1.f);
        const math::Vec3 o = projectNdc(vps[f], pos + dirs[f] * 5.f + perp * 7.5f);
        CHECK(cw.w > 0.f);
        CHECK(std::max(std::fabs(o.x), std::fabs(o.y)) > 1.f);
        // near/far: point at zNear on the axis -> ndc.z ~ -1, at zFar -> +1
        CHECK_NEAR(projectNdc(vps[f], pos + dirs[f] * 0.5f).z, -1.f, 1e-3f);
        CHECK_NEAR(projectNdc(vps[f], pos + dirs[f] * 50.f).z, 1.f, 1e-2f);
    }
    // a 45-degree off-axis point is still inside (on the frustum edge)
    const math::Vec3 edge = projectNdc(vps[0], pos + math::Vec3{10.f, 10.f, 0.f});
    CHECK_NEAR(std::fabs(edge.y), 1.f, 1e-2f);

    // --- snapLightVPToTexels: reference lands on a texel center, idempotent -----------
    const Light dl = Light::directional({0.55f, -1.f, 0.3f}, {1.f, 1.f, 1.f}, 2.f);
    const math::Vec3 center{0.f, 0.5f, 0.f};
    const math::Mat4 vp = lightViewProj(dl, center, 7.f);
    const int size = 1024;
    const math::Mat4 snapped = snapLightVPToTexels(vp, size, center);
    auto texelOf = [&](const math::Mat4& m, const math::Vec3& p) {
        const math::Vec3 n = projectNdc(m, p);
        return math::Vec2{(n.x * 0.5f + 0.5f) * size, (n.y * 0.5f + 0.5f) * size};
    };
    const math::Vec2 t = texelOf(snapped, center);
    CHECK_NEAR(t.x, std::round(t.x), 1e-2f);
    CHECK_NEAR(t.y, std::round(t.y), 1e-2f);
    // snapping again changes nothing (idempotent)
    const math::Vec2 t2 = texelOf(snapLightVPToTexels(snapped, size, center), center);
    CHECK_NEAR(t2.x, t.x, 1e-2f);
    CHECK_NEAR(t2.y, t.y, 1e-2f);
    // a different reference point can also be snapped (moving target, e.g. per cascade)
    const math::Vec3 ref2{0.123f, 0.5f, -0.456f};
    const math::Vec2 t3 = texelOf(snapLightVPToTexels(vp, size, ref2), ref2);
    CHECK_NEAR(t3.x, std::round(t3.x), 1e-2f);
    CHECK_NEAR(t3.y, std::round(t3.y), 1e-2f);
    // degenerate inputs are passed through
    const math::Mat4 same = snapLightVPToTexels(vp, 0, center);
    CHECK_NEAR(same(0, 0), vp(0, 0), 1e-6f);

    // --- hemisphereKernel: count, hemisphere, unit length, deterministic ---------------
    const auto k1 = hemisphereKernel(32, 42u);
    const auto k2 = hemisphereKernel(32, 42u);
    const auto k3 = hemisphereKernel(32, 43u);
    CHECK(k1.size() == 32);
    bool deterministic = true, seedDiffers = false;
    for (std::size_t i = 0; i < k1.size(); ++i) {
        const float len = math::length(k1[i]);
        CHECK(len <= 1.f + 1e-4f);
        CHECK(k1[i].z >= 0.f && k1[i].z <= 1.f);
        deterministic = deterministic && k1[i] == k2[i];
        seedDiffers = seedDiffers || !(k1[i] == k3[i]);
    }
    CHECK(deterministic);
    CHECK(seedDiffers);
    CHECK(hemisphereKernel(0, 1u).empty());

    // --- poissonDisk: count, radius <= 1, deterministic -------------------------------
    const auto d1 = poissonDisk(16, 7u);
    const auto d2 = poissonDisk(16, 7u);
    const auto d3 = poissonDisk(16, 8u);
    CHECK(d1.size() == 16);
    bool det = true, diff = false;
    for (std::size_t i = 0; i < d1.size(); ++i) {
        CHECK(math::length(d1[i]) <= 1.f + 1e-4f);
        det = det && d1[i] == d2[i];
        diff = diff || !(d1[i] == d3[i]);
    }
    CHECK(det);
    CHECK(diff);
    CHECK(poissonDisk(-3, 1u).empty());
}
