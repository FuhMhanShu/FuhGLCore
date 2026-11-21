// Text overlay demo (F19): renders a small lit 3D scene through ForwardPipeline and
// draws an on-screen HUD with the embedded bitmap font via render::TextRenderer —
// title, a colored status line and a live FPS counter (render::FpsCounter).
//
// With --screenshot one deterministic frame is rendered headless (surfaceless EGL)
// into an FBO and written as PNG (used by the CI docs job for the tutorial image).
//
// Usage: text [--screenshot out.png] [--width W] [--height H]

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
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
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
    if (!window.create(headless ? "text-screenshot" : "FuhGLCore - text overlay",
                       args.width, args.height)) {
        std::fprintf(stderr, "text: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "text: need GL >= 3.3\n");
        return 2;
    }

    using namespace fuhgl::render;

    Mesh boxMesh, boxMesh2;
    shapes::box(boxMesh, {0.8f, 0.8f, 0.8f}, {0.85f, 0.55f, 0.25f});
    shapes::box(boxMesh2, {0.45f, 0.45f, 0.45f}, {0.35f, 0.65f, 0.95f});
    Material boxMat = materials::matte({0.85f, 0.55f, 0.25f});
    Material boxMat2 = materials::matte({0.35f, 0.65f, 0.95f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.2f, 4.5f};
    cam.pitch = -0.12f;

    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    scene.lights.push_back({{3.f, 4.f, 5.f}, {1.f, 0.95f, 0.85f}, 30.f});
    scene.lights.push_back(Light::directional({-0.4f, -1.f, -0.3f}, {0.35f, 0.4f, 0.5f}, 0.6f));
    scene.items.push_back({&boxMesh, &boxMat, fuhgl::math::translate({-0.9f, 0.f, 0.f})});
    scene.items.push_back({&boxMesh2, &boxMat2, fuhgl::math::translate({0.9f, -0.3f, -0.6f})});

    ForwardPipeline pipeline;
    if (!pipeline.create()) {
        std::fprintf(stderr, "text: forward pipeline failed\n");
        return 1;
    }

    TextRenderer text;
    if (!text.create()) {
        std::fprintf(stderr, "text: TextRenderer create failed\n");
        return 1;
    }

    fuhgl::Framebuffer shotTarget;
    fuhgl::Texture shotTex(fuhgl::Texture::Target::Tex2D);
    fuhgl::Renderbuffer shotDepth;
    if (headless) {
        shotTex.bind();
        shotTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, args.width, args.height);
        shotDepth.create();
        shotDepth.bind();
        shotDepth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, args.width, args.height);
        shotTarget.create();
        shotTarget.setSize(args.width, args.height);
        shotTarget.bind();
        shotTarget.attachTexture2D(0, shotTex);
        shotTarget.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, shotDepth);
        if (!shotTarget.isComplete()) {
            std::fprintf(stderr, "text: shot FBO incomplete\n");
            return 1;
        }
        pipeline.setTarget(&shotTarget);
    }

    FpsCounter fps;
    double lastTime = window.time();

    auto drawHud = [&](float fpsValue) {
        char line[128];
        text.begin(scene.width, scene.height);
        text.draw("FuhGLCore - text overlay", 16.f, 14.f, 2.f, {1.f, 1.f, 1.f, 1.f});
        std::snprintf(line, sizeof(line), "FPS: %.1f", fpsValue);
        text.draw(line, 16.f, 44.f, 2.f, {0.45f, 0.95f, 0.45f, 1.f});
        text.draw("embedded 8x8 bitmap font - no external files", 16.f, 74.f, 1.f, {0.75f, 0.75f, 0.8f, 1.f});
        std::snprintf(line, sizeof(line), "res: %dx%d  GL %d.%d", scene.width, scene.height,
                      fuhgl::gl::version().major, fuhgl::gl::version().minor);
        text.draw(line, 16.f, scene.height - 28.f, 1.f, {0.65f, 0.7f, 0.85f, 1.f});
        text.end();
    };

    if (headless) {
        pipeline.render(scene);
        fuhgl::gl::flush();
        shotTarget.bind();
        drawHud(60.0f); // deterministic value for the screenshot
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "text: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "text: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     args.width, args.height, fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    cam.attach(window);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
        cam.onKey(key, action);
    });
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        fps.frame(now);
        cam.update(static_cast<float>(now - lastTime));
        lastTime = now;
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        fuhgl::Framebuffer::unbind();
        pipeline.render(scene);
        drawHud(fps.fps());
        window.swapBuffers();
    }
    return 0;
}
