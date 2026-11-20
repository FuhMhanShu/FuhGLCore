// Deferred rendering demo: a few lit boxes rendered through fuhgl::render::DeferredPipeline
// (or ForwardPipeline with --forward). Windowed by default; with --screenshot it renders
// one deterministic frame headless (surfaceless EGL) into an FBO and writes a PNG —
// used by the CI docs job for the tutorial screenshots.
//
// Usage: deferred [--forward] [--msaa N] [--screenshot out.png] [--width W] [--height H]
// Controls (windowed): WASD + mouse, Space/Ctrl up/down, Shift sprint, 1/2/3 cameras, Esc quit.

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
    bool forward = false;
    int msaa = 0;
    std::string screenshot;
    int width = 800, height = 600;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--forward") == 0) a.forward = true;
        else if (std::strcmp(argv[i], "--msaa") == 0 && i + 1 < argc) a.msaa = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) a.width = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) a.height = std::atoi(argv[++i]);
    }
    return a;
}

fuhgl::render::SceneView makeScene(fuhgl::Camera& cam, float aspect, int w, int h,
                                   std::vector<fuhgl::render::Mesh>& meshes) {
    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = w;
    scene.height = h;
    scene.aspect = aspect;

    meshes.resize(3);
    fuhgl::render::shapes::box(meshes[0], {1.0f, 1.0f, 1.0f}, {0.9f, 0.3f, 0.2f});
    fuhgl::render::shapes::box(meshes[1], {0.6f, 0.6f, 0.6f}, {0.2f, 0.8f, 0.4f});
    fuhgl::render::shapes::box(meshes[2], {0.5f, 1.4f, 0.5f}, {0.3f, 0.4f, 0.95f});

    scene.items.push_back({&meshes[0], nullptr, fuhgl::math::translate({-1.4f, 0.f, 0.f})});
    scene.items.push_back({&meshes[1], nullptr, fuhgl::math::translate({0.f, 0.2f, -1.0f}) * fuhgl::math::rotateY(0.7f)});
    scene.items.push_back({&meshes[2], nullptr, fuhgl::math::translate({1.5f, 0.4f, 0.3f})});

    scene.lights.push_back({{3.f, 3.f, 4.f}, {1.f, 0.9f, 0.7f}, 25.f});
    scene.lights.push_back({{-4.f, 2.f, -2.f}, {0.4f, 0.6f, 1.f}, 18.f});
    return scene;
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
    if (!window.create(headless ? "deferred-screenshot" : "FuhGLCore - deferred demo",
                       args.width, args.height)) {
        std::fprintf(stderr, "deferred: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "deferred: need GL >= 3.3\n");
        return 2;
    }

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.2f, 5.f};
    cam.pitch = -0.12f;

    std::vector<fuhgl::render::Mesh> meshes;
    const float aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    fuhgl::render::SceneView scene = makeScene(cam, aspect, args.width, args.height, meshes);

    fuhgl::render::DeferredPipeline deferred;
    fuhgl::render::ForwardPipeline forward;
    fuhgl::Framebuffer forwardTarget;
    if (args.forward) {
        if (!forward.create()) { std::fprintf(stderr, "forward pipeline failed\n"); return 1; }
        if (headless) {
            forwardTarget.create();
            forwardTarget.setSize(args.width, args.height);
            forwardTarget.bind();
            // attach a color texture
            // (reuse deferred's output texture is not possible; create one)
        }
    } else {
        if (!deferred.create(args.width, args.height, args.msaa)) {
            std::fprintf(stderr, "deferred pipeline failed\n");
            return 1;
        }
    }

    // offscreen target for headless screenshot (forward) 
    fuhgl::Texture shotTex(fuhgl::Texture::Target::Tex2D);
    fuhgl::Renderbuffer shotDepth;
    if (headless && args.forward) {
        shotTex.bind();
        shotTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, args.width, args.height);
        shotDepth.create();
        shotDepth.bind();
        shotDepth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, args.width, args.height);
        forwardTarget.bind();
        forwardTarget.attachTexture2D(0, shotTex);
        forwardTarget.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, shotDepth);
        if (!forwardTarget.isComplete()) { std::fprintf(stderr, "shot FBO incomplete\n"); return 1; }
        forward.setTarget(&forwardTarget);
    }

    auto renderOnce = [&]() {
        if (args.forward)
            forward.render(scene);
        else
            deferred.render(scene);
    };

    if (headless) {
        renderOnce();
        fuhgl::gl::flush();
        // read back
        if (args.forward) forwardTarget.bind(); else deferred.output().bind();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "deferred: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "deferred: wrote %s (%dx%d, GL %d.%d, %s)\n", args.screenshot.c_str(),
                     args.width, args.height, fuhgl::gl::version().major, fuhgl::gl::version().minor,
                     args.forward ? "forward" : "deferred");
        return 0;
    }

    // windowed loop
    if (args.forward) forward.setTarget(nullptr); // render straight to the window
    cam.attach(window);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
        cam.onKey(key, action);
    });
    fuhgl::gl::enable(fuhgl::gl::Cap::DepthTest);

    double lastTime = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        cam.update(static_cast<float>(now - lastTime));
        lastTime = now;

        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);

        if (args.forward) {
            fuhgl::Framebuffer::unbind();
            forward.render(scene);
        } else {
            deferred.render(scene);
            fuhgl::Framebuffer::blitToDefault(deferred.output(), scene.width, scene.height);
        }
        window.swapBuffers();
    }
    return 0;
}
