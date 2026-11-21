// Headless smoke test for F16 ForwardRenderer: renders a floor + caster via
// ForwardRenderer with shadows enabled and verifies the shadow blob gets darker;
// then verifies exponential fog pushes far floor pixels toward the fog color.
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
    if (!window.create("gl_smoke_forward", 192, 192)) {
        std::fprintf(stderr, "gl_smoke_forward: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_forward: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 192, H = 192;
    fuhgl::render::ForwardRenderer renderer;
    if (!renderer.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_forward: renderer create failed\n");
        return 1;
    }
    renderer.enableShadows(256);
    renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 7.f);
    renderer.setAmbient({0.10f, 0.10f, 0.11f});
    renderer.setClearColor({0.05f, 0.06f, 0.09f});
    renderer.addLight(fuhgl::render::Light::directional({0.55f, -1.f, 0.3f}, {1.f, 0.9f, 0.75f}, 1.5f));

    fuhgl::render::Mesh floorMesh, caster;
    fuhgl::render::shapes::box(floorMesh, {6.f, 0.25f, 6.f}, {0.6f, 0.6f, 0.62f});
    fuhgl::render::shapes::box(caster, {0.6f, 1.2f, 0.6f}, {0.9f, 0.35f, 0.25f});
    std::vector<fuhgl::render::DrawItem> items = {
        {&floorMesh, nullptr, fuhgl::math::translate({0.f, -0.25f, 0.f})},
        {&caster, nullptr, fuhgl::math::translate({-0.5f, 1.2f, 0.2f})},
    };

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 4.f, 8.f};
    cam.pitch = -0.38f;

    auto renderImage = [&](bool shadows) {
        if (shadows) renderer.enableShadows(256); else renderer.disableShadows();
        renderer.render(cam, items, W, H);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
        renderer.target()->bind();
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };

    const std::vector<unsigned char> withShadow = renderImage(true);
    const std::vector<unsigned char> noShadow = renderImage(false);
    int darker = 0, lit = 0;
    for (size_t i = 0; i < withShadow.size(); i += 4) {
        const int d = static_cast<int>(noShadow[i]) - static_cast<int>(withShadow[i]);
        if (d > 12) ++darker;
        if (noShadow[i] > 100) ++lit;
    }
    std::fprintf(stderr, "gl_smoke_forward: shadow darker=%d litFloorPx=%d\n", darker, lit);
    expect(lit > 500, "floor lit through ForwardRenderer");
    expect(darker > 150, "ForwardRenderer auto shadow pass darkens a blob");

    // fog: bright red fog color, high density -> far floor goes red
    renderer.setFog({1.f, 0.f, 0.f}, 0.3f);
    const std::vector<unsigned char> fogged = renderImage(false);
    int redFar = 0, redFarUnfogged = 0;
    for (size_t o = 0; o < fogged.size(); o += 8) { // every 2nd pixel
        if (fogged[o] > 140 && fogged[o + 1] < 70) ++redFar;
        if (noShadow[o] > 140 && noShadow[o + 1] < 70) ++redFarUnfogged;
    }
    std::fprintf(stderr, "gl_smoke_forward: fog redFar=%d unfogged redFar=%d\n", redFar, redFarUnfogged);
    expect(redFar > 300, "fog pushes far floor pixels toward the red fog color");
    expect(redFarUnfogged < redFar / 4, "without fog those pixels are not red");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_forward: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_forward: OK\n");
    return 0;
}
