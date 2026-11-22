// Headless smoke test for F41 LOD: a two-level LodMesh (two boxes with different vertex
// counts) in front of the camera. Near camera -> level 0, far camera -> level 1 (checked
// via DeferredRenderer::lastLodLevel()); with lodEnabled = false always level 0.
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
    if (!window.create("gl_smoke_lod", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_lod: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_lod: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 64, H = 64;
    fuhgl::render::DeferredRenderer r;
    if (!r.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_lod: renderer create failed\n");
        return 1;
    }
    r.ssaoEnabled = false;
    r.godRaysEnabled = false;
    r.ssrEnabled = false;
    r.taaEnabled = false;
    r.volFogEnabled = false;
    r.dofEnabled = false;

    // two LOD levels of the same box (same geometry is fine — we check the selected level)
    fuhgl::render::Mesh lod0, lod1;
    fuhgl::render::shapes::box(lod0, {0.5f, 0.5f, 0.5f}, {0.8f, 0.4f, 0.2f});
    fuhgl::render::shapes::box(lod1, {0.5f, 0.5f, 0.5f}, {0.8f, 0.4f, 0.2f});
    fuhgl::render::LodMesh lod;
    std::vector<fuhgl::render::Mesh> lv;
    lv.push_back(std::move(lod0));
    lv.push_back(std::move(lod1));
    if (!lod.create(std::move(lv), {8.f})) { // switch at 8 units
        std::fprintf(stderr, "gl_smoke_lod: lod create failed\n");
        return 1;
    }
    expect(lod.levelCount() == 2, "lod has 2 levels");
    expect(r.effectSupported(fuhgl::render::Effect::Lod), "LOD always supported (CPU)");

    fuhgl::FirstPersonCamera cam;
    fuhgl::render::SceneView scene;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.3f, 0.3f, 0.3f};
    scene.items.push_back({&lod.level(0), nullptr, fuhgl::math::translate({0.f, 0.f, -3.f}), &lod});
    scene.lights.push_back(fuhgl::render::Light::directional({0.3f, -1.f, 0.2f}, {1.f, 1.f, 1.f}, 2.f));

    // near: camera 3 units from the box -> dist 3 < 8 -> level 0
    scene.camera = &cam;
    cam.position = {0.f, 0.f, 0.f};
    cam.pitch = 0.f;
    r.render(scene);
    std::fprintf(stderr, "gl_smoke_lod: near lastLodLevel=%d\n", r.lastLodLevel());
    expect(r.lastLodLevel() == 0, "near camera selects LOD level 0");

    // far: camera 30 units away -> dist 30 >= 8 -> level 1
    cam.position = {0.f, 0.f, 27.f};
    r.render(scene);
    std::fprintf(stderr, "gl_smoke_lod: far lastLodLevel=%d\n", r.lastLodLevel());
    expect(r.lastLodLevel() == 1, "far camera selects LOD level 1");

    // disabled: always level 0
    r.lodEnabled = false;
    expect(!r.effectEnabled(fuhgl::render::Effect::Lod), "Effect::Lod toggle off");
    r.render(scene);
    std::fprintf(stderr, "gl_smoke_lod: disabled lastLodLevel=%d\n", r.lastLodLevel());
    expect(r.lastLodLevel() == -1, "lodEnabled=false -> no LOD selection (base mesh)");
    r.lodEnabled = true;

    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_lod: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_lod: OK\n");
    return 0;
}
