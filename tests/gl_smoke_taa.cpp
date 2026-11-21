// Headless smoke test for F32 TAA in DeferredRenderer: with taaEnabled the G-buffer is
// rendered with Halton subpixel jitter and accumulated over frames. Measured in HDR float
// on the post-graph resources ("ssr" = raw jittered frame, "taa" = accumulated), because
// the subpixel jitter is quantized away in the 8-bit LDR output.
// Skips cleanly without GL >= 3.3.

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

double meanAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); i += 4) s += std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
    return s / (a.size() / 4);
}
double meanAbs(const std::vector<float>& a) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); i += 4) s += std::abs(static_cast<double>(a[i]));
    return s / (a.size() / 4);
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_taa", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_taa: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_taa: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 128, H = 128;
    fuhgl::render::DeferredRenderer r;
    if (!r.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_taa: renderer create failed\n");
        return 1;
    }
    r.ssaoEnabled = false;
    r.godRaysEnabled = false;
    r.ssrEnabled = false;

    // high-frequency albedo on a large floor so the subpixel jitter changes many pixels
    fuhgl::Texture noiseTex = fuhgl::render::procedural::makeTexture(fuhgl::render::procedural::noise(128, 128, 7u, 4));
    fuhgl::render::Material floorMat = fuhgl::render::materials::pbrTextured(noiseTex, {1.f, 1.f, 1.f}, 0.f, 0.9f);
    fuhgl::render::Mesh floorMesh, boxMesh;
    fuhgl::render::shapes::box(floorMesh, {6.f, 0.25f, 6.f}, {1.f, 1.f, 1.f});
    fuhgl::render::shapes::box(boxMesh, {0.8f, 0.8f, 0.8f}, {0.8f, 0.7f, 0.6f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.2f, 3.2f};
    cam.pitch = -0.5f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.15f, 0.15f, 0.17f};
    scene.items.push_back({&floorMesh, &floorMat, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&boxMesh, nullptr, fuhgl::math::translate({0.f, 0.4f, -0.5f})});
    scene.lights.push_back(fuhgl::render::Light::directional({0.4f, -1.f, 0.3f}, {1.f, 0.95f, 0.85f}, 1.2f));

    // read an HDR (float) resource of the post graph
    fuhgl::Framebuffer hdrFbo;
    hdrFbo.create();
    hdrFbo.setSize(W, H);
    auto readHdr = [&](const char* res) {
        const fuhgl::Texture* t = r.postGraph().texture(res);
        std::vector<float> px(static_cast<size_t>(W) * H * 4, 0.f);
        if (t) {
            hdrFbo.bind();
            hdrFbo.attachTexture2D(0, *t);
            fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_FLOAT, px.data());
        }
        return px;
    };
    auto renderFrame = [&]() {
        r.render(scene);
        fuhgl::gl::flush();
    };

    // TAA off: two frames must be identical (no jitter) and the readback non-zero
    r.taaEnabled = false;
    renderFrame();
    const auto rawOffA = readHdr("ssr");
    renderFrame();
    const auto rawOffB = readHdr("ssr");
    const double lumOff = meanAbs(rawOffA);
    const double dOff = meanAbsDiff(rawOffA, rawOffB);
    std::fprintf(stderr, "gl_smoke_taa: lumOff=%.4f dOff=%.6f\n", lumOff, dOff);
    expect(lumOff > 0.005, "HDR readback is non-zero (scene visible)");
    expect(dOff < 0.001, "TAA off: consecutive frames are identical (no jitter)");

    // TAA on: raw jittered frames differ, the first accumulated frame equals the raw
    // frame (uAlpha = 1), and the accumulation stabilizes over frames
    r.taaEnabled = true;
    std::vector<std::vector<float>> raws, accs;
    for (int i = 0; i < 11; ++i) {
        renderFrame();
        raws.push_back(readHdr("ssr"));
        accs.push_back(readHdr("taa"));
        if (i > 0)
            std::fprintf(stderr, "gl_smoke_taa: frame %d dRaw=%.6f dAcc=%.6f\n", i,
                         meanAbsDiff(raws[i], raws[i - 1]), meanAbsDiff(accs[i], accs[i - 1]));
    }

    const double dRaw = meanAbsDiff(raws[0], raws[1]);
    const double dFirst = meanAbsDiff(accs[0], raws[0]);
    const double dAcc = meanAbsDiff(accs[9], accs[10]);
    std::fprintf(stderr, "gl_smoke_taa: dRaw=%.4f dFirst=%.6f dAcc=%.6f\n", dRaw, dFirst, dAcc);
    expect(dRaw > 0.002, "subpixel jitter changes the raw HDR frame");
    expect(dFirst < 0.001, "first TAA frame resolves with alpha=1 (equals the raw frame)");
    expect(dAcc < dRaw * 0.5, "TAA accumulation stabilizes across frames");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_taa: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_taa: OK\n");
    return 0;
}
