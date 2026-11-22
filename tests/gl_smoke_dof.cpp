// Headless smoke test for F36 DoF in DeferredRenderer: a near box sits on the focus
// plane, a far box beyond the focus range. With dofEnabled the far box's edges must get
// blurrier (lower gradient energy in its screen window) while the near box stays sharp.
// Measured on the HDR "dof" graph resource. Skips without GL >= 3.3.

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

// mean horizontal gradient energy (edge sharpness) inside a window around (cx, cy)
double edgeEnergy(const std::vector<float>& img, int W, int H, int cx, int cy, int half) {
    double s = 0.0;
    int n = 0;
    for (int y = std::max(1, cy - half); y < std::min(H - 1, cy + half); ++y)
        for (int x = std::max(1, cx - half); x < std::min(W - 1, cx + half); ++x) {
            const size_t a = (static_cast<size_t>(y) * W + x) * 4;
            const size_t b = (static_cast<size_t>(y) * W + x + 1) * 4;
            s += std::abs(static_cast<double>(img[a]) - static_cast<double>(img[b]));
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
    if (!window.create("gl_smoke_dof", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_dof: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_dof: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 128, H = 128;
    fuhgl::render::DeferredRenderer r;
    if (!r.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_dof: renderer create failed\n");
        return 1;
    }
    r.ssaoEnabled = false;
    r.godRaysEnabled = false;
    r.ssrEnabled = false;
    r.taaEnabled = false;

    fuhgl::render::Mesh floorMesh, nearBox, farBox;
    fuhgl::render::shapes::box(floorMesh, {8.f, 0.25f, 8.f}, {0.5f, 0.5f, 0.52f});
    fuhgl::render::shapes::box(nearBox, {0.5f, 0.5f, 0.5f}, {0.85f, 0.3f, 0.25f});
    fuhgl::render::shapes::box(farBox, {0.5f, 0.5f, 0.5f}, {0.3f, 0.5f, 0.85f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.f, 4.f};
    cam.pitch = -0.12f;

    const fuhgl::math::Vec3 nearPos{-1.2f, 0.5f, -1.0f};
    const fuhgl::math::Vec3 farPos{1.2f, 0.5f, -7.0f};
    const float focusDist = fuhgl::math::length(nearPos - cam.position);

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.15f, 0.15f, 0.17f};
    scene.items.push_back({&floorMesh, nullptr, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&nearBox, nullptr, fuhgl::math::translate(nearPos)});
    scene.items.push_back({&farBox, nullptr, fuhgl::math::translate(farPos)});
    scene.lights.push_back(fuhgl::render::Light::directional({0.4f, -1.f, 0.3f}, {1.f, 0.95f, 0.85f}, 1.2f));

    // read the HDR "dof" graph resource
    fuhgl::Framebuffer hdrFbo;
    hdrFbo.create();
    hdrFbo.setSize(W, H);
    auto readDof = [&]() {
        const fuhgl::Texture* t = r.postGraph().texture("dof");
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
    const auto [nx, ny] = project(nearPos);
    const auto [fx, fy] = project(farPos);
    std::fprintf(stderr, "gl_smoke_dof: near window at (%d,%d), far window at (%d,%d)\n", nx, ny, fx, fy);

    r.dofEnabled = false;
    r.render(scene);
    fuhgl::gl::flush();
    const auto imgOff = readDof();

    r.dofEnabled = true;
    r.dofFocusDist = focusDist;
    r.dofFocusRange = 3.f;
    r.dofStrength = 16.f;
    r.dofTaps = 24;
    r.render(scene);
    fuhgl::gl::flush();
    const auto imgOn = readDof();

    const double eNearOff = edgeEnergy(imgOff, W, H, nx, ny, 10);
    const double eNearOn = edgeEnergy(imgOn, W, H, nx, ny, 10);
    const double eFarOff = edgeEnergy(imgOff, W, H, fx, fy, 10);
    const double eFarOn = edgeEnergy(imgOn, W, H, fx, fy, 10);
    std::fprintf(stderr, "gl_smoke_dof: near E off=%.4f on=%.4f | far E off=%.4f on=%.4f\n", eNearOff, eNearOn,
                 eFarOff, eFarOn);
    expect(eFarOff > 0.01, "far box has visible edges with DoF off");
    expect(eFarOn < eFarOff * 0.7, "DoF blurs the out-of-focus far box");
    expect(eNearOn > eNearOff * 0.6, "focused near box stays sharp");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_dof: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_dof: OK\n");
    return 0;
}
