// Headless smoke test for F43: render::DemoScene feeds both ready-to-use renderers —
// DeferredRenderer (full FX stack) and ForwardRenderer (HDR post chain) — from the same
// scene. Verifies both produce a lit image, that fillScene() wired lights/env correctly,
// and that no GL errors occur. Skips without GL >= 3.3.

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

double meanLum(fuhgl::Framebuffer& fbo, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4, 0);
    fbo.bind();
    fuhgl::gl::readPixels(0, 0, w, h, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
    double s = 0;
    for (size_t i = 0; i < px.size(); i += 4)
        s += 0.2126 * px[i] + 0.7152 * px[i + 1] + 0.0722 * px[i + 2];
    return s / (px.size() / 4);
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_demo", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_demo: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_demo: context below GL 3.3 — SKIP\n");
        return 77;
    }

    using namespace fuhgl::render;
    const int W = 160, H = 120;

    DemoScene demo;
    if (!demo.create(DemoMode::Day)) {
        std::fprintf(stderr, "gl_smoke_demo: DemoScene::create failed\n");
        return 1;
    }
    expect(demo.mode() == DemoMode::Day, "demo mode day");
    expect(demo.environment() == nullptr, "no IBL environment in day mode");
    expect(demo.heightAt(0.f, 17.f) == demo.heightAt(0.f, 17.f), "heightAt deterministic");

    fuhgl::FirstPersonCamera cam;
    demo.applyStartPose(cam);

    SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = static_cast<float>(W) / static_cast<float>(H);
    demo.fillScene(scene);
    expect(!scene.items.empty(), "fillScene provides draw items");
    expect(!scene.lights.empty(), "fillScene provides lights");

    // 1) deferred renderer with the full demo configuration
    DeferredRenderer dr;
    if (!dr.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_demo: DeferredRenderer::create failed\n");
        return 1;
    }
    demo.configure(dr);
    expect(dr.csmShadowsEnabled(), "deferred: CSM shadows configured by the demo");
    for (int i = 0; i < 4; ++i) dr.render(scene); // let TAA history accumulate
    fuhgl::gl::flush();
    const double lumDef = meanLum(dr.output(), W, H);
    std::fprintf(stderr, "gl_smoke_demo: deferred lum=%.1f\n", lumDef);
    expect(lumDef > 5.0, "deferred renderer renders the demo scene lit");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors (deferred)");

    // 2) forward renderer with the HDR post chain, same scene
    ForwardRenderer fr;
    if (!fr.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_demo: ForwardRenderer::create failed\n");
        return 1;
    }
    demo.configure(fr);
    expect(fr.csmShadowsEnabled(), "forward: CSM shadows configured by the demo");
    expect(fr.postFxEnabled(), "forward: post FX enabled by the demo");
    expect(fr.lights().size() == scene.lights.size(), "forward: demo lights applied");
    for (int i = 0; i < 4; ++i) fr.render(cam, scene.items, W, H);
    fuhgl::gl::flush();
    const double lumFwd = meanLum(*fr.target(), W, H);
    std::fprintf(stderr, "gl_smoke_demo: forward lum=%.1f\n", lumFwd);
    expect(lumFwd > 5.0, "forward renderer renders the demo scene lit (post chain)");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors (forward)");

    // 3) night mode bakes an IBL environment
    DemoScene night;
    if (!night.create(DemoMode::Night)) {
        std::fprintf(stderr, "gl_smoke_demo: night DemoScene::create failed\n");
        return 1;
    }
    expect(night.environment() != nullptr, "night mode provides the IBL environment");
    expect(std::string(night.title()) == "night", "night title");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_demo: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_demo: OK\n");
    return 0;
}
