// Headless smoke test for F32 TAA in DeferredRenderer: with taaEnabled the G-buffer is
// rendered with Halton subpixel jitter and accumulated over frames; the accumulated
// image must converge toward the unjittered (taa off) image.
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

double meanAbsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    double s = 0.0;
    for (size_t i = 0; i < a.size(); i += 4) s += std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
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

    fuhgl::render::Mesh boxMesh;
    fuhgl::render::shapes::box(boxMesh, {1.f, 1.f, 1.f}, {0.8f, 0.7f, 0.6f});
    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.f, 5.f};
    cam.pitch = -0.15f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.08f, 0.08f, 0.09f};
    scene.items.push_back({&boxMesh, nullptr, fuhgl::math::Mat4{}});
    scene.lights.push_back(fuhgl::render::Light::directional({0.4f, -1.f, 0.3f}, {1.f, 0.95f, 0.85f}, 1.2f));

    // read the HDR (float) "taa" resource of the post graph — subpixel jitter is only
    // visible in float precision, not in the quantized LDR output
    fuhgl::Framebuffer hdrFbo;
    hdrFbo.create();
    hdrFbo.setSize(W, H);
    auto renderImg = [&]() {
        r.render(scene);
        fuhgl::gl::flush();
        const fuhgl::Texture* taaTex = r.postGraph().texture("taa");
        std::vector<float> px(static_cast<size_t>(W) * H * 4, 0.f);
        hdrFbo.bind();
        hdrFbo.attachTexture2D(0, *taaTex);
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_FLOAT, px.data());
        return px;
    };

    r.taaEnabled = false;
    const auto imgOff = renderImg();
    r.taaEnabled = true;
    const auto imgFirst = renderImg(); // frame 0: alpha = 1, pure jittered frame
    for (int i = 0; i < 9; ++i) renderImg();
    const auto imgAcc = renderImg(); // after 10 jittered frames

    const double dFirst = meanAbsDiff(imgFirst, imgOff);
    const double dAcc = meanAbsDiff(imgAcc, imgOff);
    std::fprintf(stderr, "gl_smoke_taa: meanAbsDiff(first jittered vs off)=%.2f, accumulated vs off=%.2f\n", dFirst, dAcc);
    expect(dFirst > 0.002, "subpixel jitter changes the raw HDR frame");
    expect(dAcc < dFirst * 0.8, "TAA accumulation converges toward the unjittered image");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_taa: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_taa: OK\n");
    return 0;
}
