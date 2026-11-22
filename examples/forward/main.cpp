// Default forward renderer demo (F16): render::ForwardRenderer with a directional sun
// (auto shadow map), a point light, distance fog and a procedural floor texture.
// Windowed by default; with --screenshot it renders one deterministic frame headless
// (surfaceless EGL) and writes a PNG — used by the CI docs job.
//
// Usage: forward [--screenshot out.png] [--width W] [--height H] [--no-shadows] [--no-fog] [--fx]
// --fx enables the F42 post chain on the forward renderer (HDR pass + bloom/god rays/TAA/DoF/vol fog).
// Controls (windowed): Esc quit.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include "../screenshot/png_writer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string screenshot;
    int width = 800, height = 600;
    bool shadows = true, fog = true, fx = false;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--no-shadows") == 0) a.shadows = false;
        else if (std::strcmp(argv[i], "--no-fog") == 0) a.fog = false;
        else if (std::strcmp(argv[i], "--fx") == 0) a.fx = true;
    }
    return a;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    const bool headless = !args.screenshot.empty();
    if (headless) {
#ifndef _WIN32
        if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
        if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif
    }

    fuhgl::Window window;
    if (!window.create(headless ? "forward-screenshot" : "FuhGLCore - forward renderer demo", args.width, args.height)) {
        std::fprintf(stderr, "forward: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "forward: need GL >= 3.3\n");
        return 2;
    }

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 4.2f, 9.5f};
    cam.pitch = -0.35f;

    fuhgl::render::ForwardRenderer renderer;
    // with --fx (windowed) the renderer needs an own sized target for the HDR post chain
    const int initW = headless ? args.width : (args.fx ? window.framebufferWidth() : 0);
    const int initH = headless ? args.height : (args.fx ? window.framebufferHeight() : 0);
    if (!renderer.create(initW, initH)) {
        std::fprintf(stderr, "forward: renderer create failed\n");
        return 1;
    }
    if (args.shadows) {
        renderer.enableShadows(1024);
        renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 8.f);
    }
    if (args.fog) renderer.setFog({0.55f, 0.65f, 0.8f}, 0.028f);
    if (args.fx) { // F42: post chain on the forward renderer
        renderer.enablePostFX(true);
        renderer.postChain().bloomStrength = 0.55f;
        renderer.postChain().rayWeight = 0.4f;
        renderer.postChain().taaEnabled = true;
        renderer.postChain().dofEnabled = true;
        renderer.postChain().dofFocusDist = 6.5f;
        renderer.postChain().dofFocusRange = 4.f;
        renderer.postChain().dofStrength = 10.f;
        renderer.postChain().volFogEnabled = true;
        renderer.postChain().volFogDensity = 0.012f;
    }
    renderer.setAmbient({0.10f, 0.10f, 0.12f});
    renderer.setClearColor({0.55f, 0.65f, 0.8f});
    renderer.addLight(fuhgl::render::Light::directional({0.55f, -1.f, 0.3f}, {1.f, 0.9f, 0.72f}, 1.7f));
    renderer.addLight(fuhgl::render::Light::point({-2.5f, 3.f, 1.f}, {0.45f, 0.65f, 1.f}, 12.f, 10.f));

    // procedural checker floor texture
    fuhgl::Texture floorTex = fuhgl::render::procedural::makeTexture(
        fuhgl::render::procedural::checker(256, 256, 8, {0.35f, 0.35f, 0.38f}, {0.75f, 0.75f, 0.78f}));
    fuhgl::render::Material floorMat = fuhgl::render::materials::textured(floorTex, {1.f, 1.f, 1.f});

    std::vector<fuhgl::render::Mesh> meshes(5);
    fuhgl::render::shapes::box(meshes[0], {7.f, 0.25f, 7.f}, {1.f, 1.f, 1.f}); // floor (textured)
    fuhgl::render::shapes::box(meshes[1], {0.7f, 1.4f, 0.7f}, {0.9f, 0.35f, 0.25f});
    fuhgl::render::shapes::box(meshes[2], {0.9f, 0.9f, 0.9f}, {0.3f, 0.55f, 0.9f});
    fuhgl::render::shapes::box(meshes[3], {0.5f, 2.2f, 0.5f}, {0.35f, 0.8f, 0.45f});
    fuhgl::render::shapes::box(meshes[4], {1.2f, 0.4f, 1.2f}, {0.85f, 0.75f, 0.3f});

    std::vector<fuhgl::render::DrawItem> items = {
        {&meshes[0], &floorMat, fuhgl::math::translate({0.f, -0.25f, 0.f})},
        {&meshes[1], nullptr, fuhgl::math::translate({-2.2f, 1.4f, 0.4f})},
        {&meshes[2], nullptr, fuhgl::math::translate({1.8f, 0.9f, -1.6f}) * fuhgl::math::rotateY(0.5f)},
        {&meshes[3], nullptr, fuhgl::math::translate({0.4f, 2.2f, 1.8f})},
        {&meshes[4], nullptr, fuhgl::math::translate({3.2f, 0.4f, 1.2f}) * fuhgl::math::rotateY(0.9f)},
    };

    auto renderOnce = [&]() {
        if (headless)
            renderer.render(cam, items, args.width, args.height);
        else {
            const int w = window.framebufferWidth(), h = window.framebufferHeight();
            if (w > 0 && h > 0) {
                renderer.render(cam, items, w, h);
                if (args.fx) renderer.blitToScreen(); // post chain wrote into target()
            }
        }
    };

    if (headless) {
        for (int i = 0; i < 8; ++i) renderOnce(); // let TAA history accumulate
        fuhgl::gl::flush();
        renderer.target()->bind();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "forward: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "forward: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(), args.width, args.height,
                     fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
    });
    while (window.isOpen()) {
        renderOnce();
        window.swapBuffers();
        window.pollEvents();
    }
    return 0;
}
