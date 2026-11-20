// Headless deferred-pipeline smoke test: renders one lit box through
// DeferredPipeline into its output FBO and verifies the center pixel is lit
// and a corner pixel shows the sky gradient. Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_deferred", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_deferred: no GL context — skipping\n");
        return 0;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_deferred: context below GL 3.3 — skipping\n");
        return 0;
    }

    const int W = 256, H = 256;
    fuhgl::render::DeferredPipeline pipeline;
    if (!pipeline.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_deferred: pipeline create failed\n");
        return 1;
    }

    fuhgl::render::Mesh box;
    fuhgl::render::shapes::box(box, {1.f, 1.f, 1.f}, {0.9f, 0.3f, 0.2f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.2f, 5.f};
    cam.pitch = -0.12f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.items.push_back({&box, nullptr, fuhgl::math::Mat4{}});
    scene.lights.push_back({{3.f, 3.f, 4.f}, {1.f, 0.9f, 0.7f}, 25.f});
    scene.lights.push_back({{-4.f, 2.f, -2.f}, {0.4f, 0.6f, 1.f}, 18.f});

    pipeline.render(scene);
    fuhgl::gl::flush();

    pipeline.output().bind();
    unsigned char center[4] = {0, 0, 0, 0}, corner[4] = {0, 0, 0, 0};
    fuhgl::gl::readPixels(W / 2, H / 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, center);
    fuhgl::gl::readPixels(2, 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, corner);
    std::fprintf(stderr, "gl_smoke_deferred: center=(%d,%d,%d,%d) corner=(%d,%d,%d,%d)\n",
                 center[0], center[1], center[2], center[3], corner[0], corner[1], corner[2], corner[3]);

    // center should be the lit box (reddish, clearly not the sky), corner the sky (bluish)
    const bool centerLit = center[0] > 40 && center[3] == 255;
    const bool cornerSky = corner[2] > corner[0]; // sky is blue-dominant
    if (!centerLit || !cornerSky) {
        std::fprintf(stderr, "gl_smoke_deferred: FAIL\n");
        return 1;
    }
    return 0;
}
