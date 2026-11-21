// Headless smoke test for F30 cascaded shadow maps: floor + caster box, directional
// light, ForwardRenderer with enableCsmShadows() — the shadow blob must get darker
// both in a near view (cascade 0) and in a far view (a higher cascade).
// Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_csm", 192, 192)) {
        std::fprintf(stderr, "gl_smoke_csm: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_csm: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 192, H = 192;
    fuhgl::render::ForwardRenderer renderer;
    if (!renderer.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_csm: renderer create failed\n");
        return 1;
    }
    renderer.enableCsmShadows(1024, 4);
    renderer.setShadowZRange(0.5f, 120.f);
    renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 12.f);
    renderer.setAmbient({0.10f, 0.10f, 0.11f});
    renderer.setClearColor({0.05f, 0.06f, 0.09f});
    renderer.addLight(fuhgl::render::Light::directional({0.55f, -1.f, 0.3f}, {1.f, 0.9f, 0.75f}, 1.5f));
    expect(renderer.csmShadowsEnabled() && renderer.cascadeShadowMap() &&
               renderer.cascadeShadowMap()->cascadeCount() == 4,
           "CSM created with 4 cascades");

    fuhgl::render::Mesh floorMesh, caster;
    fuhgl::render::shapes::box(floorMesh, {8.f, 0.25f, 8.f}, {0.6f, 0.6f, 0.62f});
    fuhgl::render::shapes::box(caster, {0.7f, 1.4f, 0.7f}, {0.9f, 0.35f, 0.25f});
    std::vector<fuhgl::render::DrawItem> items = {
        {&floorMesh, nullptr, fuhgl::math::translate({0.f, -0.25f, 0.f})},
        {&caster, nullptr, fuhgl::math::translate({-0.5f, 1.4f, 0.2f})},
    };

    fuhgl::FirstPersonCamera cam;

    auto renderImage = [&](const fuhgl::math::Vec3& camPos, float pitch) {
        cam.position = camPos;
        cam.yaw = 0.f;
        cam.pitch = pitch;
        renderer.render(cam, items, W, H);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
        renderer.target()->bind();
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };

    auto countDarker = [](const std::vector<unsigned char>& a, const std::vector<unsigned char>& b) {
        int darker = 0, lit = 0;
        for (size_t i = 0; i < a.size(); i += 4) {
            if (static_cast<int>(b[i]) - static_cast<int>(a[i]) > 12) ++darker;
            if (b[i] > 100) ++lit;
        }
        return std::pair<int, int>{darker, lit};
    };

    // near view (cascade 0 covers it) and far view (higher cascade)
    const auto nearCsm = renderImage({0.f, 4.f, 8.f}, -0.38f);
    renderer.disableCsmShadows();
    const auto nearNoShadow = renderImage({0.f, 4.f, 8.f}, -0.38f);
    renderer.enableCsmShadows(1024, 4);
    renderer.setShadowZRange(0.5f, 120.f);
    renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 12.f);

    const auto farCsm = renderImage({0.f, 10.f, 45.f}, -0.22f);
    renderer.disableCsmShadows();
    const auto farNoShadow = renderImage({0.f, 10.f, 45.f}, -0.22f);

    const auto [nearDarker, nearLit] = countDarker(nearCsm, nearNoShadow);
    const auto [farDarker, farLit] = countDarker(farCsm, farNoShadow);
    std::fprintf(stderr, "gl_smoke_csm: near darker=%d lit=%d | far darker=%d lit=%d\n", nearDarker, nearLit,
                 farDarker, farLit);
    expect(nearLit > 500, "near view: floor is lit");
    expect(nearDarker > 100, "near view: CSM darkens the shadow blob (cascade 0)");
    expect(farLit > 300, "far view: floor is lit");
    expect(farDarker > 60, "far view: CSM still shadows at distance (higher cascade)");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_csm: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_csm: OK\n");
    return 0;
}
