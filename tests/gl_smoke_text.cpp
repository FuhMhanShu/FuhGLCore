// Headless smoke test for F19 TextRenderer: renders white and colored text into an
// FBO on a dark background and verifies via readback that glyph pixels were written
// (and that the empty region stays untouched). Skips cleanly without GL >= 3.3.

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
    if (!window.create("gl_smoke_text", 256, 128)) {
        std::fprintf(stderr, "gl_smoke_text: no GL context — skipping\n");
        return 0;
    }
    if (!version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_text: context below GL 3.3 — skipping\n");
        return 0;
    }
    const auto& f = functions();
    expect(lastError() == Error::NoError, "clean context");

    const int W = 256, H = 128;
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

    f.glViewport(0, 0, W, H);
    f.glClearColor(0.02f, 0.02f, 0.05f, 1.f);
    f.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    fuhgl::render::TextRenderer text;
    expect(text.create(), "TextRenderer::create");
    // simulate pipeline state: culling + depth test left enabled (must not hide text)
    f.glEnable(GL_CULL_FACE);
    f.glEnable(GL_DEPTH_TEST);
    text.begin(W, H);
    text.draw("FPS: 60", 8.f, 8.f, 2.f, {1.f, 1.f, 1.f, 1.f});
    text.draw("red", 8.f, 32.f, 2.f, {1.f, 0.2f, 0.2f, 1.f});
    text.end();
    f.glFlush();
    expect(lastError() == Error::NoError, "no GL error after text render");

    std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
    f.glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    // glReadPixels returns rows bottom-up; flip y so we can use screen coords (y down)
    auto lum = [&](int x, int y) {
        const size_t o = (static_cast<size_t>(H - 1 - y) * W + x) * 4;
        return px[o] + px[o + 1] + px[o + 2];
    };
    // count bright pixels in the two text rows (y 8..24 and 32..48, x 8..120)
    int brightTop = 0, brightRed = 0, brightEmpty = 0;
    for (int y = 8; y < 24; ++y)
        for (int x = 8; x < 120; ++x)
            if (lum(x, y) > 200) ++brightTop;
    for (int y = 32; y < 48; ++y)
        for (int x = 8; x < 40; ++x) {
            const size_t o = (static_cast<size_t>(H - 1 - y) * W + x) * 4;
            if (px[o] > 150 && px[o + 1] < 100 && px[o + 2] < 100) ++brightRed;
        }
    for (int y = 96; y < 120; ++y)
        for (int x = 8; x < 120; ++x)
            if (lum(x, y) > 200) ++brightEmpty;
    expect(brightTop > 20, "white text pixels rendered (FPS: 60)");
    expect(brightRed > 5, "red text pixels rendered");
    expect(brightEmpty == 0, "empty region untouched");

    if (g_failures == 0) std::fprintf(stderr, "gl_smoke_text: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
