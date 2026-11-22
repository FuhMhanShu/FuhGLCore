// Renderer demo (F43): the render::DemoScene landscape run through EITHER ready-to-use
// renderer — the same scene, one call per frame:
//
//   --renderer deferred (default): DeferredRenderer — CSM + SSAO + HDR + bloom + god rays
//                                   + TAA + SSR + DoF + vol fog (the showcase look)
//   --renderer forward:             ForwardRenderer — CSM + HDR post chain (bloom, god rays,
//                                   TAA, DoF, vol fog) via enablePostFX()
//
// DemoScene::configure(renderer) applies the matching effect stack, DemoScene::fillScene()
// provides the per-frame SceneView. See tutorial 45.
//
// With --screenshot a few deterministic frames are rendered headless (surfaceless EGL,
// TAA history accumulates) into the renderer output and written as PNG.
//
// Usage: demo [--renderer forward|deferred] [--mode day|sunset|night]
//             [--screenshot out.png] [--width W] [--height H]
// Controls (windowed): WASD+mouse, Esc quits.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include "../screenshot/png_writer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string screenshot;
    std::string renderer = "deferred";
    std::string mode = "day";
    int width = 960, height = 540;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) a.renderer = argv[++i];
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) a.mode = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
    }
    return a;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    const bool useForward = args.renderer == "forward";
    if (!useForward && args.renderer != "deferred") {
        std::fprintf(stderr, "demo: unknown --renderer '%s' (use forward|deferred)\n", args.renderer.c_str());
        return 2;
    }
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
    if (!window.create(headless ? "demo-screenshot" : "FuhGLCore - renderer demo",
                       args.width, args.height)) {
        std::fprintf(stderr, "demo: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "demo: need GL >= 3.3\n");
        return 2;
    }

    using namespace fuhgl::render;

    // --- one demo scene, two renderers ---------------------------------------------------
    DemoScene demo;
    if (!demo.create(mode)) {
        std::fprintf(stderr, "demo: DemoScene create failed\n");
        return 1;
    }

    fuhgl::FirstPersonCamera cam;
    demo.applyStartPose(cam);

    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    demo.fillScene(scene);

    DeferredRenderer deferred;
    ForwardRenderer forward;
    fuhgl::Framebuffer* out = nullptr;
    std::function<void()> renderFrame;
    std::function<void(int, int)> resizeRenderer;
    if (useForward) {
        if (!forward.create(args.width, args.height)) {
            std::fprintf(stderr, "demo: ForwardRenderer create failed\n");
            return 1;
        }
        demo.configure(forward);
        renderFrame = [&] { forward.render(cam, scene.items, scene.width, scene.height); };
        resizeRenderer = [&](int w, int h) { forward.resize(w, h); };
        out = forward.target();
    } else {
        if (!deferred.create(args.width, args.height)) {
            std::fprintf(stderr, "demo: DeferredRenderer create failed\n");
            return 1;
        }
        demo.configure(deferred);
        renderFrame = [&] { deferred.render(scene); };
        resizeRenderer = [&](int w, int h) { deferred.resize(w, h); };
        out = &deferred.output();
    }

    TextRenderer text;
    if (!text.create()) {
        std::fprintf(stderr, "demo: TextRenderer create failed\n");
        return 1;
    }
    auto drawHud = [&](float fpsValue) {
        char line[220];
        text.begin(scene.width, scene.height);
        std::snprintf(line, sizeof(line), "FuhGLCore renderer demo - %s (%s renderer)", demo.title(),
                      useForward ? "forward" : "deferred");
        text.draw(line, 14.f, 12.f, 2.f, {1.f, 1.f, 1.f, 1.f});
        std::snprintf(line, sizeof(line), "%s | FPS: %.1f",
                      useForward ? "forward: CSM + HDR post chain (bloom + god rays + TAA + DoF + vol fog)"
                                 : "deferred: CSM + SSAO + HDR + bloom + god rays + TAA + SSR + vol fog + DoF",
                      fpsValue);
        text.draw(line, 14.f, 40.f, 1.f, {0.85f, 0.88f, 0.95f, 1.f});
        text.end();
    };

    if (headless) {
        for (int i = 0; i < 8; ++i) renderFrame(); // let TAA history accumulate
        fuhgl::gl::flush();
        out->bind();
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
            std::fprintf(stderr, "demo: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "demo: wrote %s (renderer=%s mode=%s, %dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     args.renderer.c_str(), demo.title(), args.width, args.height,
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
    int lastWidth = scene.width, lastHeight = scene.height;
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        fps.frame(now);
        cam.update(static_cast<float>(now - lastTime));
        lastTime = now;
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        if (scene.width != lastWidth || scene.height != lastHeight) {
            resizeRenderer(scene.width, scene.height);
            lastWidth = scene.width;
            lastHeight = scene.height;
        }
        renderFrame();
        out->bind();
        drawHud(fps.fps());
        if (useForward) forward.blitToScreen();
        else fuhgl::Framebuffer::blitToDefault(deferred.output(), scene.width, scene.height);
        window.swapBuffers();
    }
    return 0;
}
