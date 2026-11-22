// README showcase (F25, refactored in F43): the render::DemoScene landscape rendered with
// the DeferredRenderer and its full effect stack — cascaded shadow maps (F30), SSAO, HDR
// lighting with a visible sun disk, bloom, crepuscular god rays, distance fog, TAA (F32),
// SSR (F33), depth of field (F36) and volumetric fog / light shafts (F37), plus a gradient
// sky. The scene itself lives in the library (render/demo.hpp); `examples/demo` runs the
// same DemoScene through the ForwardRenderer. Three looks: --mode day | sunset | night.
// CI renders all three as README screenshots.
//
// With --screenshot a few deterministic frames are rendered headless (surfaceless EGL,
// TAA history accumulates) into the renderer output and written as PNG.
//
// Usage: showcase [--screenshot out.png] [--mode day|sunset|night] [--width W] [--height H]
// Controls (windowed): WASD+mouse, Esc quits.

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
    std::string mode = "day";
    int width = 960, height = 540;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) a.mode = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
    }
    return a;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    const auto mode = fuhgl::render::demoModeFromName(args.mode);
    const bool headless = !args.screenshot.empty();
    if (headless) {
#ifndef _WIN32
        if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
        if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif
    }

    fuhgl::Window window;
    if (!window.create(headless ? "showcase-screenshot" : "FuhGLCore - showcase",
                       args.width, args.height)) {
        std::fprintf(stderr, "showcase: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "showcase: need GL >= 3.3\n");
        return 2;
    }

    using namespace fuhgl::render;

    // --- renderer with the full effect stack (configured from the demo scene) -----------
    DeferredRenderer renderer;
    if (!renderer.create(args.width, args.height)) {
        std::fprintf(stderr, "showcase: renderer create failed\n");
        return 1;
    }

    // --- the demo scene (terrain, CSG props, instanced grass, lights, modes) ------------
    DemoScene demo;
    if (!demo.create(mode)) {
        std::fprintf(stderr, "showcase: DemoScene create failed\n");
        return 1;
    }
    demo.configure(renderer);

    fuhgl::FirstPersonCamera cam;
    demo.applyStartPose(cam);

    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    demo.fillScene(scene);

    TextRenderer text;
    if (!text.create()) {
        std::fprintf(stderr, "showcase: TextRenderer create failed\n");
        return 1;
    }
    const bool night = mode == DemoMode::Night, sunset = mode == DemoMode::Sunset;
    auto drawHud = [&](float fpsValue) {
        char line[220];
        text.begin(scene.width, scene.height);
        text.draw(night ? "FuhGLCore showcase - night (IBL + SSR)"
                : sunset ? "FuhGLCore showcase - sunset (god rays)"
                         : "FuhGLCore showcase - day",
                  14.f, 12.f, 2.f, {1.f, 1.f, 1.f, 1.f});
        std::snprintf(line, sizeof(line),
                      "deferred renderer: CSM + SSAO + HDR + bloom + god rays + TAA + SSR + vol fog + DoF | FPS: %.1f",
                      fpsValue);
        text.draw(line, 14.f, 40.f, 1.f, {0.85f, 0.88f, 0.95f, 1.f});
        text.end();
    };

    if (headless) {
        for (int i = 0; i < 8; ++i) renderer.render(scene); // let TAA history accumulate
        fuhgl::gl::flush();
        renderer.output().bind();
        drawHud(60.0f);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "showcase: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "showcase: wrote %s (mode=%s, %dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     demo.title(), args.width, args.height,
                     fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    cam.attach(window);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
        cam.onKey(key, action);
    });
    FpsCounter fps;
    double lastTime = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        fps.frame(now);
        cam.update(static_cast<float>(now - lastTime));
        lastTime = now;
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        if (scene.width != renderer.width() || scene.height != renderer.height())
            renderer.resize(scene.width, scene.height);
        renderer.render(scene);
        renderer.output().bind();
        drawHud(fps.fps());
        fuhgl::Framebuffer::blitToDefault(renderer.output(), scene.width, scene.height);
        window.swapBuffers();
    }
    return 0;
}
