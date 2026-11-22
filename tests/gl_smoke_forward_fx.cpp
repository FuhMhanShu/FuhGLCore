// Headless smoke test for F42: ForwardRenderer with the post chain enabled (HDR forward
// pass -> PostChain bloom/god rays/TAA/DoF/vol fog -> LDR target). Verifies the target is
// lit, the effect API reports support correctly (SSR/SSAO never on forward), the plain
// (post off) path still works, and no GL errors occur. Skips without GL >= 3.3.

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
    if (!window.create("gl_smoke_forward_fx", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_forward_fx: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_forward_fx: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 128, H = 128;
    fuhgl::render::ForwardRenderer r;
    if (!r.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_forward_fx: create failed\n");
        return 1;
    }
    const bool gl33 = fuhgl::gl::version().atLeast(3, 3);
    expect(!r.effectSupported(fuhgl::render::Effect::Ssr), "SSR never supported on forward (no G-buffer)");
    expect(!r.effectSupported(fuhgl::render::Effect::Ssao), "SSAO never supported on forward");
    expect(r.effectSupported(fuhgl::render::Effect::Bloom) == gl33, "bloom support matches GL version");
    expect(!r.effectEnabled(fuhgl::render::Effect::Bloom), "post effects off until enablePostFX");

    fuhgl::render::Mesh boxMesh, floorMesh;
    fuhgl::render::shapes::box(boxMesh, {0.5f, 0.5f, 0.5f}, {0.85f, 0.45f, 0.2f});
    fuhgl::render::shapes::box(floorMesh, {6.f, 0.2f, 6.f}, {0.5f, 0.5f, 0.52f});
    std::vector<fuhgl::render::DrawItem> items;
    items.push_back({&floorMesh, nullptr, fuhgl::math::translate({0.f, -0.2f, 0.f})});
    items.push_back({&boxMesh, nullptr, fuhgl::math::translate({0.f, 0.5f, -1.5f})});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.f, 3.5f};
    cam.pitch = -0.15f;

    r.setLights({fuhgl::render::Light::directional({0.3f, -1.f, 0.25f}, {1.f, 0.95f, 0.85f}, 2.2f)});
    r.setAmbient({0.2f, 0.2f, 0.22f});
    r.setClearColor({0.05f, 0.06f, 0.09f});

    auto readTarget = [&]() {
        std::vector<unsigned char> px(size_t(W) * H * 4, 0);
        r.target()->bind();
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };
    auto meanLum = [](const std::vector<unsigned char>& px) {
        double s = 0;
        for (size_t i = 0; i < px.size(); i += 4)
            s += 0.2126 * px[i] + 0.7152 * px[i + 1] + 0.0722 * px[i + 2];
        return s / (px.size() / 4);
    };

    // 1) plain forward path (post off) — unchanged behavior
    r.render(cam, items, W, H);
    fuhgl::gl::flush();
    const double lumPlain = meanLum(readTarget());
    std::fprintf(stderr, "gl_smoke_forward_fx: plain lum=%.1f\n", lumPlain);
    expect(lumPlain > 5.0, "plain forward path renders a lit scene");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors (plain path)");

    // 2) post chain on with all effects
    r.enablePostFX(true);
    r.postChain().setAllEffects(true);
    r.postChain().dofFocusDist = 4.5f;
    r.postChain().volFogDensity = 0.03f;
    expect(r.effectEnabled(fuhgl::render::Effect::Bloom) == gl33, "bloom enabled with post FX on");
    expect(r.effectEnabled(fuhgl::render::Effect::DoF) == gl33, "DoF enabled with post FX on");
    for (int i = 0; i < 4; ++i) r.render(cam, items, W, H); // let TAA history accumulate
    fuhgl::gl::flush();
    const double lumFx = meanLum(readTarget());
    std::fprintf(stderr, "gl_smoke_forward_fx: postfx lum=%.1f\n", lumFx);
    expect(lumFx > 5.0, "post chain renders a lit scene into the LDR target");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors (post FX path)");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_forward_fx: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_forward_fx: OK\n");
    return 0;
}
