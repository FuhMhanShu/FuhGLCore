// Default deferred renderer demo (F17): DeferredRenderer with SSAO, HDR lighting,
// PCF shadows, bloom, crepuscular god rays, ACES tone mapping — plus a parallax/bump
// mapped brick floor (procedural height + derived normal map).
// Windowed by default; with --screenshot it renders one deterministic frame headless
// (surfaceless EGL) and writes a PNG — used by the CI docs job.
//
// Usage: postfx [--screenshot out.png] [--width W] [--height H] [--no-fx] [--no-shadows]
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
    bool fx = true, shadows = true;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--no-fx") == 0) a.fx = false;
        else if (std::strcmp(argv[i], "--no-shadows") == 0) a.shadows = false;
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
    if (!window.create(headless ? "postfx-screenshot" : "FuhGLCore - deferred renderer + post FX demo",
                       args.width, args.height)) {
        std::fprintf(stderr, "postfx: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "postfx: need GL >= 3.3\n");
        return 2;
    }

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 3.2f, 9.f};
    cam.pitch = -0.28f;

    fuhgl::render::DeferredRenderer renderer;
    if (!renderer.create(args.width, args.height)) {
        std::fprintf(stderr, "postfx: renderer create failed\n");
        return 1;
    }
    if (args.shadows) {
        renderer.enableShadows(1024);
        renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 8.f);
    }
    if (!args.fx) {
        renderer.ssaoEnabled = false;
        renderer.bloomStrength = 0.f;
        renderer.godRaysEnabled = false;
    }
    renderer.exposure = 1.0f;
    renderer.bloomStrength = args.fx ? 0.5f : 0.f;
    renderer.rayWeight = 0.28f;
    renderer.pipeline().skyTop = {0.10f, 0.26f, 0.68f};
    renderer.pipeline().skyBottom = {0.5f, 0.6f, 0.82f};

    // parallax/bump brick floor
    const fuhgl::render::procedural::Image heightImg = fuhgl::render::procedural::noise(256, 256, 42u, 5);
    fuhgl::Texture heightTex = fuhgl::render::procedural::makeTexture(heightImg);
    fuhgl::Texture normalTex = fuhgl::render::procedural::makeNormalMap(heightImg, 3.f);
    fuhgl::Texture bricksTex = fuhgl::render::procedural::makeTexture(
        fuhgl::render::procedural::bricks(256, 256, 8, 12, {0.5f, 0.26f, 0.18f}, {0.72f, 0.69f, 0.64f}, 0.1f));
    fuhgl::render::Material floorMat = fuhgl::render::materials::parallax(bricksTex, heightTex, 0.05f, 20);
    floorMat.normalMap = &normalTex;

    std::vector<fuhgl::render::Mesh> meshes(4);
    fuhgl::render::shapes::box(meshes[0], {7.f, 0.25f, 7.f}, {1.f, 1.f, 1.f});   // floor
    fuhgl::render::shapes::box(meshes[1], {0.8f, 1.5f, 0.8f}, {0.85f, 0.4f, 0.3f});
    fuhgl::render::shapes::box(meshes[2], {0.6f, 0.6f, 0.6f}, {0.3f, 0.55f, 0.9f});
    fuhgl::render::shapes::box(meshes[3], {0.3f, 0.3f, 0.3f}, {1.f, 1.f, 1.f});   // lamp
    fuhgl::render::Material lampMat = fuhgl::render::materials::emissive({1.f, 0.8f, 0.45f}, 10.f);

    // sun low in front-right of the camera: visible disk + long shadows + god rays
    const fuhgl::render::Light sun = fuhgl::render::Light::directional({-0.35f, -0.3f, 1.f}, {1.f, 0.88f, 0.7f}, 2.2f);

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    scene.clearColor = {0.04f, 0.05f, 0.08f};
    scene.ambient = {0.16f, 0.16f, 0.18f};
    scene.fogColor = {0.4f, 0.5f, 0.72f};
    scene.fogDensity = 0.012f;
    scene.items.push_back({&meshes[0], &floorMat, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&meshes[1], nullptr, fuhgl::math::translate({-2.0f, 1.5f, -0.6f})});
    scene.items.push_back({&meshes[2], nullptr, fuhgl::math::translate({1.7f, 0.6f, -1.8f}) * fuhgl::math::rotateY(0.6f)});
    scene.items.push_back({&meshes[3], &lampMat, fuhgl::math::translate({0.6f, 0.3f, 1.6f})});
    scene.lights.push_back(sun);
    scene.lights.push_back(fuhgl::render::Light::point({-3.f, 2.5f, 2.f}, {0.45f, 0.6f, 1.f}, 12.f, 11.f));

    auto renderOnce = [&]() {
        renderer.render(scene);
        if (!headless)
            fuhgl::Framebuffer::blitToDefault(renderer.output(), window.framebufferWidth(), window.framebufferHeight());
    };

    if (headless) {
        renderer.render(scene);
        fuhgl::gl::flush();
        renderer.output().bind();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "postfx: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "postfx: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(), args.width, args.height,
                     fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
    });
    while (window.isOpen()) {
        renderer.render(scene);
        window.swapBuffers();
        window.pollEvents();
    }
    return 0;
}
