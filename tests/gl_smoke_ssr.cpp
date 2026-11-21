// Headless smoke test for F33 SSR in DeferredRenderer: a bright emissive box standing
// on a dim floor is reflected in the floor by the screen-space-reflection pass, so the
// floor region gets brighter with ssrEnabled than without.
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

double meanRed(const std::vector<unsigned char>& px, int w, int h, int y0, int y1) {
    double s = 0.0;
    int n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < w; ++x) {
            s += px[(static_cast<size_t>(y) * w + x) * 4];
            ++n;
        }
    return n ? s / n : 0.0;
}

int countBrighter(const std::vector<unsigned char>& on, const std::vector<unsigned char>& off, int w, int h,
                  int y0, int y1, int margin) {
    int n = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = (static_cast<size_t>(y) * w + x) * 4;
            if (static_cast<int>(on[i]) - static_cast<int>(off[i]) > margin) ++n;
        }
    return n;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_ssr", 160, 160)) {
        std::fprintf(stderr, "gl_smoke_ssr: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_ssr: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 160, H = 160;
    fuhgl::render::DeferredRenderer r;
    if (!r.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_ssr: renderer create failed\n");
        return 1;
    }
    r.ssaoEnabled = false;
    r.godRaysEnabled = false;
    r.bloomStrength = 0.f;

    fuhgl::render::Mesh floorMesh, lampMesh;
    fuhgl::render::shapes::box(floorMesh, {10.f, 0.25f, 10.f}, {0.55f, 0.55f, 0.58f});
    fuhgl::render::shapes::box(lampMesh, {0.5f, 1.f, 0.5f}, {0.f, 0.f, 0.f});
    fuhgl::render::Material lampMat = fuhgl::render::materials::emissive({1.f, 0.9f, 0.7f}, 6.f);

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 3.f, 7.f};
    cam.pitch = -0.45f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.04f, 0.05f, 0.08f};
    scene.ambient = {0.05f, 0.05f, 0.06f};
    scene.items.push_back({&floorMesh, nullptr, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&lampMesh, &lampMat, fuhgl::math::translate({-1.f, 1.f, -1.f})});
    scene.lights.push_back(fuhgl::render::Light::directional({0.3f, -1.f, 0.4f}, {1.f, 1.f, 1.f}, 0.6f));

    auto renderImg = [&]() {
        r.render(scene);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
        r.output().bind();
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };

    r.ssrEnabled = false;
    const auto imgOff = renderImg();
    r.ssrEnabled = true;
    r.ssrStrength = 1.0f;
    r.ssrMaxSteps = 48;
    r.ssrThickness = 0.6f;
    const auto imgOn = renderImg();

    // floor region = bottom half of the frame; count pixels the reflection brightened
    const double floorOff = meanRed(imgOff, W, H, H / 2, H);
    const double floorOn = meanRed(imgOn, W, H, H / 2, H);
    const int brighter = countBrighter(imgOn, imgOff, W, H, H / 2, H, 8);
    std::fprintf(stderr, "gl_smoke_ssr: floor mean red off=%.1f on=%.1f brighterPx=%d\n", floorOff, floorOn, brighter);
    expect(floorOff > 10, "floor is visible and lit without SSR");
    expect(brighter > 30, "SSR brightens floor pixels with the lamp's reflection");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_ssr: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_ssr: OK\n");
    return 0;
}
