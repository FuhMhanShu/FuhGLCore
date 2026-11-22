// Headless smoke test for F37 volumetric fog in DeferredRenderer: with volFogEnabled a far
// floor window must change noticeably (extinction + in-scattering) while a near window
// barely changes; with volFogShadows + CSM the floor inside a caster's shadow gets less
// in-scattered sun light (darker) than without. Measured on the HDR "volfog" graph
// resource. Skips without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

// mean per-channel abs diff between two images inside a window around (cx, cy)
double meanDiff(const std::vector<float>& a, const std::vector<float>& b, int W, int H,
                int cx, int cy, int half) {
    double s = 0.0;
    int n = 0;
    for (int y = std::max(0, cy - half); y < std::min(H, cy + half); ++y)
        for (int x = std::max(0, cx - half); x < std::min(W, cx + half); ++x) {
            const size_t i = (static_cast<size_t>(y) * W + x) * 4;
            s += (std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i])) +
                  std::abs(static_cast<double>(a[i + 1]) - static_cast<double>(b[i + 1])) +
                  std::abs(static_cast<double>(a[i + 2]) - static_cast<double>(b[i + 2]))) / 3.0;
            ++n;
        }
    return n > 0 ? s / n : 0.0;
}

// mean luminance inside a window around (cx, cy)
double meanLum(const std::vector<float>& img, int W, int H, int cx, int cy, int half) {
    double s = 0.0;
    int n = 0;
    for (int y = std::max(0, cy - half); y < std::min(H, cy + half); ++y)
        for (int x = std::max(0, cx - half); x < std::min(W, cx + half); ++x) {
            const size_t i = (static_cast<size_t>(y) * W + x) * 4;
            s += 0.2126 * img[i] + 0.7152 * img[i + 1] + 0.0722 * img[i + 2];
            ++n;
        }
    return n > 0 ? s / n : 0.0;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_volfog", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_volfog: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_volfog: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 128, H = 128;
    fuhgl::render::DeferredRenderer r;
    if (!r.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_volfog: renderer create failed\n");
        return 1;
    }
    r.enableCsmShadows(1024, 4); // F30 — needed for volFogShadows
    r.setShadowZRange(0.5f, 60.f);
    r.setShadowSceneBounds({0.f, 1.f, -6.f}, 14.f);
    r.setShadowStrength(1.f);
    r.ssaoEnabled = false;
    r.godRaysEnabled = false;
    r.ssrEnabled = false;
    r.taaEnabled = false;

    fuhgl::render::Mesh floorMesh, casterMesh;
    fuhgl::render::shapes::box(floorMesh, {20.f, 0.25f, 20.f}, {0.5f, 0.5f, 0.52f});
    fuhgl::render::shapes::box(casterMesh, {0.75f, 2.f, 0.2f}, {0.6f, 0.6f, 0.62f}); // tall slab at y in [0,4]

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.2f, 6.f};
    cam.pitch = -0.06f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.15f, 0.15f, 0.17f};
    scene.items.push_back({&floorMesh, nullptr, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&casterMesh, nullptr, fuhgl::math::translate({0.f, 2.f, -6.f})});
    // sun from behind-above the camera, slightly toward -z: the caster throws a shadow
    // onto the floor between it and the camera
    scene.lights.push_back(fuhgl::render::Light::directional({0.f, -1.f, 0.45f}, {1.f, 0.9f, 0.75f}, 2.5f));

    // read the HDR "volfog" graph resource
    fuhgl::Framebuffer hdrFbo;
    hdrFbo.create();
    hdrFbo.setSize(W, H);
    auto readVolFog = [&]() {
        const fuhgl::Texture* t = r.postGraph().texture("volfog");
        std::vector<float> px(static_cast<size_t>(W) * H * 4, 0.f);
        hdrFbo.bind();
        hdrFbo.attachTexture2D(0, *t);
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_FLOAT, px.data());
        return px;
    };

    // project a world position to readback pixel coords (GL origin = bottom-left)
    auto project = [&](const fuhgl::math::Vec3& p) {
        const fuhgl::math::Vec4 c = cam.viewProj(1.f) * fuhgl::math::Vec4(p.x, p.y, p.z, 1.f);
        return std::make_pair(static_cast<int>((c.x / c.w * 0.5f + 0.5f) * W),
                              static_cast<int>((c.y / c.w * 0.5f + 0.5f) * H));
    };
    const auto [fx, fy] = project({1.5f, 0.f, -18.f});   // far floor, outside the shadow
    const auto [nx, ny] = project({2.f, 0.f, 4.f});      // near floor
    const auto [sx, sy] = project({0.f, 0.f, -5.f});     // floor inside the caster's shadow
    std::fprintf(stderr, "gl_smoke_volfog: far (%d,%d) near (%d,%d) shadow (%d,%d)\n", fx, fy, nx, ny, sx, sy);

    // 1) fog off
    r.volFogEnabled = false;
    r.render(scene);
    fuhgl::gl::flush();
    const auto imgOff = readVolFog();

    // 2) fog on, no shadow attenuation
    r.volFogEnabled = true;
    r.volFogDensity = 0.06f;
    r.volFogColor = {0.75f, 0.80f, 0.88f};
    r.volFogAnisotropy = 0.3f;
    r.volFogHeight = 0.f;
    r.volFogShadows = false;
    r.volFogSteps = 24;
    r.volFogMaxDist = 120.f;
    r.render(scene);
    fuhgl::gl::flush();
    const auto imgOn = readVolFog();

    const double farDiff = meanDiff(imgOff, imgOn, W, H, fx, fy, 5);
    const double nearDiff = meanDiff(imgOff, imgOn, W, H, nx, ny, 5);
    std::fprintf(stderr, "gl_smoke_volfog: farDiff=%.4f nearDiff=%.4f\n", farDiff, nearDiff);
    expect(farDiff > 0.02, "fog changes far pixels (extinction + in-scattering)");
    expect(nearDiff < farDiff * 0.7, "near pixels stay almost unaffected");

    // 3) fog on with CSM shadow attenuation -> less in-scattering inside the shadow
    r.volFogShadows = true;
    r.render(scene);
    fuhgl::gl::flush();
    const auto imgSh = readVolFog();

    const double lumNoSh = meanLum(imgOn, W, H, sx, sy, 5);
    const double lumSh = meanLum(imgSh, W, H, sx, sy, 5);
    std::fprintf(stderr, "gl_smoke_volfog: shadow window lum noShadow=%.4f withShadow=%.4f\n", lumNoSh, lumSh);
    expect(lumNoSh > 0.01, "shadow window is visible (lit by fog in-scattering)");
    expect(lumSh < lumNoSh * 0.9, "volFogShadows darkens the shadowed fog (light shaft)");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_volfog: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_volfog: OK\n");
    return 0;
}
