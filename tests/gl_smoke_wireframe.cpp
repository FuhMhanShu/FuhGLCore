// Headless smoke test for F20 Wireframe: renders a white AABB box and a green mesh
// wireframe (box) into an FBO and verifies via readback that line pixels of both
// colors exist. Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace fuhgl::gl;

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
    if (!window.create("gl_smoke_wireframe", 256, 256)) {
        std::fprintf(stderr, "gl_smoke_wireframe: no GL context — skipping\n");
        return 0;
    }
    if (!version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_wireframe: context below GL 3.3 — skipping\n");
        return 0;
    }
    const auto& f = functions();

    const int W = 256, H = 256;
    fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);
    tex.bind();
    tex.texStorage2D(1, GL_RGBA8, W, H);
    fuhgl::Renderbuffer depth;
    depth.create();
    depth.bind();
    depth.storage(GL_DEPTH_COMPONENT24, W, H);
    fuhgl::Framebuffer fbo;
    fbo.create();
    fbo.setSize(W, H);
    fbo.bind();
    fbo.attachTexture2D(0, tex);
    fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depth);
    expect(fbo.isComplete(), "FBO complete");

    f.glClearColor(0.02f, 0.02f, 0.05f, 1.f);
    f.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.f, 4.f};
    cam.pitch = 0.f; // looks along -Z at the origin

    fuhgl::render::Wireframe wf;
    expect(wf.create(), "Wireframe::create");
    wf.addBox({-1.2f, -1.2f, -1.2f}, {1.2f, 1.2f, 1.2f}, {1.f, 1.f, 1.f});          // white AABB
    wf.addMesh(fuhgl::render::shapes::boxData({1.f, 1.f, 1.f}, {1, 1, 1}),          // green mesh edges
               fuhgl::math::Mat4{}, {0.f, 1.f, 0.f});
    expect(wf.lineCount() == 42, "42 queued lines (12 box + 30 mesh edges)");
    wf.render(cam, W, H);
    f.glFlush();
    expect(lastError() == Error::NoError, "no GL error after wireframe render");

    std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
    f.glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    int white = 0, green = 0;
    for (size_t i = 0; i + 3 < px.size(); i += 4) {
        const int r = px[i], g = px[i + 1], b = px[i + 2];
        if (r > 180 && g > 180 && b > 180) ++white;
        else if (g > 150 && r < 100 && b < 100) ++green;
    }
    expect(white > 100, "white AABB edge pixels rendered");
    expect(green > 100, "green mesh edge pixels rendered");

    if (g_failures == 0) std::fprintf(stderr, "gl_smoke_wireframe: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
