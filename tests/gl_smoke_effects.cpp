// Headless smoke test for F39/F40: both renderers expose the effect toggle API; on the CI
// GPU (llvmpipe, GL >= 3.3) all shader effects are supported, setEffectEnabled round-trips,
// effectEnabled() reflects the flag, and a frame renders with everything toggled on/off.
// Skips without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>

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
    if (!window.create("gl_smoke_effects", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_effects: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_effects: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const bool gl33 = fuhgl::gl::version().atLeast(3, 3);
    const bool compute = fuhgl::gl::queryDeviceInfo().features.compute;

    fuhgl::render::DeferredRenderer def;
    expect(def.create(64, 64), "deferred create");
    expect(def.effectSupported(fuhgl::render::Effect::DoF) == gl33, "deferred DoF support matches GL version");
    expect(def.effectSupported(fuhgl::render::Effect::GpuCulling) == compute, "deferred culling support matches compute flag");
    expect(def.effectSupported(fuhgl::render::Effect::Lod), "LOD always supported (CPU)");
    def.setEffectEnabled(fuhgl::render::Effect::DoF, true);
    expect(def.effectEnabled(fuhgl::render::Effect::DoF) == gl33, "deferred DoF toggle round-trip");
    def.setEffectEnabled(fuhgl::render::Effect::VolFog, false);
    expect(!def.effectEnabled(fuhgl::render::Effect::VolFog), "deferred volfog off");
    def.setAllEffects(true);
    expect(def.effectEnabled(fuhgl::render::Effect::Ssao) == gl33, "setAllEffects(true) respects support");
    def.setAllEffects(false);
    expect(!def.effectEnabled(fuhgl::render::Effect::Ssao) && !def.effectEnabled(fuhgl::render::Effect::Shadows),
           "setAllEffects(false) disables");

    fuhgl::render::ForwardRenderer fwd;
    expect(fwd.create(64, 64), "forward create");
    expect(fwd.effectSupported(fuhgl::render::Effect::CsmShadows) == gl33, "forward CSM support matches GL version");
    fwd.setEffectEnabled(fuhgl::render::Effect::Shadows, true);
    expect(fwd.effectEnabled(fuhgl::render::Effect::Shadows) == gl33, "forward shadows toggle round-trip");
    fwd.setAllEffects(false);
    expect(!fwd.effectEnabled(fuhgl::render::Effect::Shadows), "forward setAllEffects(false)");

    // render one frame with everything on (deferred) — must not crash / no GL errors
    fuhgl::render::Mesh box;
    fuhgl::render::shapes::box(box, {0.5f, 0.5f, 0.5f}, {0.8f, 0.4f, 0.2f});
    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.5f, 3.f};
    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = 64;
    scene.height = 64;
    scene.aspect = 1.f;
    scene.items.push_back({&box, nullptr, fuhgl::math::Mat4{}});
    scene.lights.push_back(fuhgl::render::Light::directional({0.3f, -1.f, 0.2f}, {1.f, 1.f, 1.f}, 2.f));
    def.setAllEffects(true);
    def.render(scene);
    fuhgl::gl::flush();
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors with all effects on");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_effects: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_effects: OK\n");
    return 0;
}
