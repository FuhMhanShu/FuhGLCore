#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/cam/first_person.hpp>
#include <fuhgl/render/deferred_renderer.hpp>
#include <fuhgl/render/shadow.hpp>

#include <algorithm>
#include <cmath>

using namespace fuhgl;
using namespace fuhgl::render;

namespace {
math::Vec3 projectNdc(const math::Mat4& m, const math::Vec3& p) {
    const math::Vec4 c = m * math::Vec4(p, 1.f);
    return {c.x / c.w, c.y / c.w, c.z / c.w};
}
} // namespace

void runShadowTests() {
    // --- directional light straight down: ortho box around the scene sphere --------
    const Light dl = Light::directional({0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, 2.f);
    const math::Mat4 vp = lightViewProj(dl, {0.f, 0.f, 0.f}, 5.f);
    const math::Vec3 c = projectNdc(vp, {0.f, 0.f, 0.f});
    CHECK(std::fabs(c.x) < 1e-4f && std::fabs(c.y) < 1e-4f); // scene center -> map center
    CHECK(c.z >= -1.f && c.z <= 1.f);                        // inside near/far (mid -> ~0)
    const math::Vec3 edge = projectNdc(vp, {5.f, 0.f, 0.f}); // on the ortho boundary (axis depends on up-flip)
    CHECK_NEAR(std::max(std::fabs(edge.x), std::fabs(edge.y)), 1.f, 1e-3f);
    CHECK(std::fabs(projectNdc(vp, {0.f, -100.f, 0.f}).z) > 1.f); // beyond far
    CHECK(std::fabs(projectNdc(vp, {0.f, 100.f, 0.f}).z) > 1.f);  // behind the light eye
    // no NaN even for a straight-down light (safe up-vector fallback)
    for (float v : {c.x, c.y, c.z, edge.x})
        CHECK(std::isfinite(v));

    // tilted directional light: center still centered, sphere stays inside the map
    const Light dl2 = Light::directional({0.55f, -1.f, 0.3f}, {1.f, 1.f, 1.f}, 2.f);
    const math::Mat4 vp2 = lightViewProj(dl2, {0.f, 0.5f, 0.f}, 7.f);
    const math::Vec3 c2 = projectNdc(vp2, {0.f, 0.5f, 0.f});
    CHECK(std::fabs(c2.x) < 1e-3f && std::fabs(c2.y) < 1e-3f && c2.z > 0.f && c2.z < 1.f);
    for (const math::Vec3 p : {math::Vec3{7.f, 0.5f, 0.f}, math::Vec3{-7.f, 0.5f, 0.f},
                               math::Vec3{0.f, 0.5f, 7.f}, math::Vec3{0.f, 0.5f, -7.f}}) {
        const math::Vec3 n = projectNdc(vp2, p);
        CHECK(n.x >= -1.01f && n.x <= 1.01f && n.y >= -1.01f && n.y <= 1.01f);
    }

    // --- spot light: perspective with fov = 2 * coneOuter --------------------------
    const Light sl = Light::spot({0.f, 5.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, 30.f, 0.5f, 0.8f);
    const math::Mat4 proj = lightProjection(sl, 1.f, 0.5f, 0.f);
    CHECK_NEAR(proj(0, 0), 1.f / std::tan(0.8f), 1e-4f); // fovY/2 = coneOuter
    const math::Mat4 svp = lightViewProj(sl, {0.f, 0.f, 0.f}, 5.f);
    const math::Vec3 sc = projectNdc(svp, {0.f, 0.f, 0.f});
    CHECK(std::fabs(sc.x) < 1e-4f && std::fabs(sc.y) < 1e-4f && sc.z > 0.f && sc.z < 1.f);
    // a point on the outer cone edge at view depth d maps to max(|ndc.x|,|ndc.y|) ~ 1
    const float d = 10.f;
    const math::Vec3 coneEdge{std::tan(0.8f) * d, 5.f - d, 0.f};
    const math::Vec3 ec = projectNdc(svp, coneEdge);
    CHECK_NEAR(std::max(std::fabs(ec.x), std::fabs(ec.y)), 1.f, 0.02f);
    // outside the cone -> outside the map
    const math::Vec3 outside{std::tan(0.9f) * d, 5.f - d, 0.f};
    const math::Vec3 oc = projectNdc(svp, outside);
    CHECK(std::max(std::fabs(oc.x), std::fabs(oc.y)) > 1.f);

    // --- sunScreenUV (god-ray source position) ----------------------------------------
    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.f, 5.f}; // looks down -Z
    cam.yaw = 0.f;
    cam.pitch = 0.f;
    const math::Vec2 uvFront = sunScreenUV(cam, {0.f, 0.3f, -1.f}, 100, 100); // sun in front, slightly up
    CHECK(uvFront.x > 0.4f && uvFront.x < 0.6f);
    CHECK(uvFront.y > 0.5f && uvFront.y < 0.85f); // above center
    const math::Vec2 uvBehind = sunScreenUV(cam, {0.f, 0.f, 1.f}, 100, 100); // behind the camera
    CHECK(uvBehind.x < 0.f && uvBehind.y < 0.f);
    const math::Vec2 uvCenter = sunScreenUV(cam, {0.f, 0.f, -1.f}, 100, 100);
    CHECK_NEAR(uvCenter.x, 0.5f, 1e-3f);
    CHECK_NEAR(uvCenter.y, 0.5f, 1e-3f);

    // --- F30: cascadeSplits -------------------------------------------------------------
    const auto splits = cascadeSplits(0.5f, 200.f, 4, 0.5f);
    CHECK_NEAR(splits[0], 0.5f, 1e-5f);
    CHECK_NEAR(splits[4], 200.f, 1e-2f);
    for (int i = 1; i <= 4; ++i) CHECK(splits[i] > splits[i - 1]); // strictly ascending
    const auto su = cascadeSplits(1.f, 9.f, 4, 0.f); // lambda 0 = uniform
    CHECK_NEAR(su[1], 3.f, 1e-4f);
    CHECK_NEAR(su[2], 5.f, 1e-4f);
    CHECK_NEAR(su[3], 7.f, 1e-4f);
    const auto slog = cascadeSplits(1.f, 16.f, 4, 1.f); // lambda 1 = pure log
    CHECK_NEAR(slog[1], 2.f, 1e-3f);
    CHECK_NEAR(slog[2], 4.f, 1e-3f);
    CHECK_NEAR(slog[3], 8.f, 1e-3f);
    CHECK_NEAR(cascadeSplits(1.f, 10.f, 99)[kMaxCascades], 10.f, 1e-4f); // count clamped

    // --- F30: cascadeViewProj covers its own frustum slice ------------------------------
    fuhgl::FirstPersonCamera cam2; // at origin, looking down -Z
    cam2.position = {0.f, 0.f, 0.f};
    cam2.yaw = 0.f;
    cam2.pitch = 0.f;
    const Light dl3 = Light::directional({0.3f, -1.f, 0.2f}, {1.f, 1.f, 1.f}, 2.f);
    const auto sp = cascadeSplits(0.5f, 100.f, 4);
    const math::Mat4 camProj = cam2.projection(1.f);
    const float tanHalfFovY = 1.f / camProj(1, 1);
    for (int i = 0; i < 4; ++i) {
        const math::Mat4 vp = cascadeViewProj(dl3, cam2, 1.f, sp[i], sp[i + 1], {0.f, 0.f, 0.f}, 50.f);
        // slice corners (on the camera frustum slice) map inside the cascade's ortho box
        for (float d : {sp[i], sp[i + 1]})
            for (float sx : {-1.f, 1.f})
                for (float sy : {-1.f, 1.f}) {
                    const math::Vec3 n = projectNdc(vp, {sx * tanHalfFovY * d, sy * tanHalfFovY * d, -d});
                    CHECK(n.x >= -1.02f && n.x <= 1.02f && n.y >= -1.02f && n.y <= 1.02f);
                    CHECK(std::isfinite(n.x) && std::isfinite(n.y) && std::isfinite(n.z));
                }
        // a point on the camera axis at the slice's mid depth is inside [0,1]^3
        const float d = 0.5f * (sp[i] + sp[i + 1]);
        const math::Vec3 n = projectNdc(vp, {0.f, 0.f, -d});
        CHECK(n.x >= -0.02f && n.x <= 1.02f && n.y >= -0.02f && n.y <= 1.02f && n.z >= -1.02f && n.z <= 1.02f);
    }
    // spot light falls back to the full-range VP (finite, no crash)
    const Light spot2 = Light::spot({0.f, 5.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, 30.f, 0.5f, 0.8f);
    const math::Vec3 ns = projectNdc(cascadeViewProj(spot2, cam2, 1.f, 1.f, 10.f, {0.f, 0.f, 0.f}, 5.f),
                                      {0.f, 0.f, -5.f});
    CHECK(std::isfinite(ns.x) && std::isfinite(ns.y) && std::isfinite(ns.z));
}
