// Headless smoke test for F38 heightmap terrain: build a HeightmapTerrain from a
// procedural heightmap, render one deferred frame looking down at it, verify the center
// is not black (terrain drawn) and heightAt matches the source data. Skips without GL >= 3.3.

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
    if (!window.create("gl_smoke_terrain", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_terrain: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_terrain: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 128, H = 128;
    // heightmap -> terrain
    auto img = fuhgl::render::procedural::heightmap(64, 64, 7u, 5, 0.09f);
    fuhgl::render::HeightmapTerrain terrain;
    if (!terrain.create(img, 20.f, 20.f, 6.f)) {
        std::fprintf(stderr, "gl_smoke_terrain: terrain create failed\n");
        return 1;
    }
    expect(terrain.mesh().vertexCount() == 64u * 64u, "terrain mesh has w*h vertices");
    // heightAt must match a manual bilinear sample of the image
    const float manual = fuhgl::render::heightmapSample(img, 0.5f, 0.5f) * 6.f;
    expect(std::abs(terrain.heightAt(0.f, 0.f) - manual) < 1e-4f, "heightAt(0,0) matches bilinear sample");
    expect(terrain.heightAt(100.f, 100.f) == terrain.heightAt(10.f, 10.f), "heightAt clamps outside the terrain");

    fuhgl::render::DeferredRenderer r;
    if (!r.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_terrain: renderer create failed\n");
        return 1;
    }
    r.ssaoEnabled = false;
    r.godRaysEnabled = false;
    r.ssrEnabled = false;
    r.taaEnabled = false;

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, terrain.heightAt(0.f, 8.f) + 4.f, 8.f};
    cam.pitch = -0.9f; // look down at the terrain

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.25f, 0.25f, 0.28f};
    scene.items.push_back({&terrain.mesh(), nullptr, fuhgl::math::Mat4{}});
    scene.lights.push_back(fuhgl::render::Light::directional({0.3f, -1.f, 0.2f}, {1.f, 0.95f, 0.85f}, 2.0f));

    r.render(scene);
    fuhgl::gl::flush();

    // read back the LDR output center
    std::vector<unsigned char> px(size_t(W) * H * 4, 0);
    r.output().bind();
    fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
    const size_t c = (size_t(H / 2) * W + W / 2) * 4;
    const int lum = px[c] + px[c + 1] + px[c + 2];
    std::fprintf(stderr, "gl_smoke_terrain: center px = (%d, %d, %d)\n", px[c], px[c + 1], px[c + 2]);
    expect(lum > 30, "terrain is drawn (center not black)");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_terrain: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_terrain: OK\n");
    return 0;
}
