// Headless smoke test for F14 light types: renders the top face of a box through
// ForwardPipeline with different lights and checks the center pixel brightness
// (directional down = bright, directional horizontal = ambient only, spot aimed
// down = bright, spot aimed away = dark, point above = bright).
// Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    if (!window.create("gl_smoke_lights", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_lights: no GL context — skipping\n");
        return 0;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_lights: context below GL 3.3 — skipping\n");
        return 0;
    }

    const int W = 128, H = 128;
    fuhgl::render::ForwardPipeline pipeline;
    if (!pipeline.create()) {
        std::fprintf(stderr, "gl_smoke_lights: pipeline create failed\n");
        return 1;
    }

    fuhgl::Texture shotTex(fuhgl::Texture::Target::Tex2D);
    shotTex.bind();
    shotTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, W, H);
    fuhgl::Renderbuffer depth;
    depth.create();
    depth.bind();
    depth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, W, H);
    fuhgl::Framebuffer target;
    target.create();
    target.setSize(W, H);
    target.bind();
    target.attachTexture2D(0, shotTex);
    target.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depth);
    if (!target.isComplete()) {
        std::fprintf(stderr, "gl_smoke_lights: target FBO incomplete\n");
        return 1;
    }
    pipeline.setTarget(&target);

    fuhgl::render::Mesh box;
    fuhgl::render::shapes::box(box, {1.f, 1.f, 1.f}, {0.9f, 0.9f, 0.9f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.5f, 6.f}; // look at the box's +Z front face (center of the frame)

    auto renderCenter = [&](const fuhgl::render::Light& light) {
        fuhgl::render::SceneView scene;
        scene.camera = &cam;
        scene.width = W;
        scene.height = H;
        scene.aspect = 1.f;
        scene.clearColor = {0.f, 0.f, 0.f};
        scene.ambient = {0.06f, 0.06f, 0.06f};
        scene.items.push_back({&box, nullptr, fuhgl::math::Mat4{}});
        scene.lights.push_back(light);
        pipeline.render(scene);
        fuhgl::gl::flush();
        unsigned char px[4] = {0, 0, 0, 0};
        target.bind();
        fuhgl::gl::readPixels(W / 2, H / 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
        return px[0];
    };

    const unsigned char cDirFront = renderCenter(fuhgl::render::Light::directional({0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, 2.f));
    const unsigned char cDirSide = renderCenter(fuhgl::render::Light::directional({1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, 2.f));
    const unsigned char cSpotFront = renderCenter(fuhgl::render::Light::spot({0.f, 0.5f, 4.f}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, 50.f, 0.5f, 0.8f));
    const unsigned char cSpotAway = renderCenter(fuhgl::render::Light::spot({0.f, 0.5f, 4.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, 50.f, 0.5f, 0.8f));
    const unsigned char cPoint = renderCenter(fuhgl::render::Light::point({0.f, 0.5f, 4.f}, {1.f, 1.f, 1.f}, 50.f));
    std::fprintf(stderr, "gl_smoke_lights: dirFront=%d dirSide=%d spotFront=%d spotAway=%d point=%d\n",
                 cDirFront, cDirSide, cSpotFront, cSpotAway, cPoint);

    expect(cDirFront > 200, "directional light from the camera lights the front face");
    expect(cDirSide < 60, "directional light from the side leaves only ambient on the front face");
    expect(cSpotFront > 200, "spot aimed at the front face lights the center");
    expect(cSpotAway < 60, "spot aimed away: center outside the cone stays dark");
    expect(cPoint > 200, "point light in front of the box lights the center");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_lights: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_lights: OK\n");
    return 0;
}
