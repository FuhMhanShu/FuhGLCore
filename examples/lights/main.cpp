// Lights demo (F14): one scene lit by a directional sun, a point light and a spot
// light, rendered through fuhgl::render::ForwardPipeline. Windowed by default; with
// --screenshot it renders one deterministic frame headless (surfaceless EGL) into an
// FBO and writes a PNG — used by the CI docs job for the tutorial screenshot.
//
// Usage: lights [--screenshot out.png] [--width W] [--height H]
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

fuhgl::render::SceneView makeScene(fuhgl::Camera& cam, float aspect, int w, int h,
                                   std::vector<fuhgl::render::Mesh>& meshes) {
    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = w;
    scene.height = h;
    scene.aspect = aspect;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.13f, 0.13f, 0.15f};

    meshes.resize(4);
    fuhgl::render::shapes::box(meshes[0], {6.f, 0.25f, 6.f}, {0.55f, 0.55f, 0.58f}); // floor
    fuhgl::render::shapes::box(meshes[1], {0.8f, 0.8f, 0.8f}, {0.9f, 0.35f, 0.25f});  // red box
    fuhgl::render::shapes::box(meshes[2], {0.5f, 1.3f, 0.5f}, {0.25f, 0.75f, 0.4f});  // green pillar
    fuhgl::render::shapes::box(meshes[3], {0.6f, 0.6f, 0.6f}, {0.35f, 0.45f, 0.95f}); // blue box

    scene.items.push_back({&meshes[0], nullptr, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&meshes[1], nullptr, fuhgl::math::translate({-2.2f, 0.8f, -0.5f})});
    scene.items.push_back({&meshes[2], nullptr, fuhgl::math::translate({0.3f, 1.3f, -1.8f})});
    scene.items.push_back({&meshes[3], nullptr, fuhgl::math::translate({2.0f, 0.6f, 0.8f}) * fuhgl::math::rotateY(0.6f)});

    // directional "sun" (warm), point light (cool) hovering over the red box,
    // spot light aimed at the green pillar
    scene.lights.push_back(fuhgl::render::Light::directional({-0.4f, -1.f, -0.25f}, {1.f, 0.85f, 0.65f}, 1.3f));
    scene.lights.push_back(fuhgl::render::Light::point({-2.2f, 2.6f, 0.6f}, {0.45f, 0.65f, 1.f}, 11.f, 9.f));
    scene.lights.push_back(fuhgl::render::Light::spot({0.3f, 4.5f, 1.6f}, {0.f, -1.f, -0.35f}, {1.f, 0.95f, 0.7f}, 32.f,
                                                      fuhgl::math::radians(18.f), fuhgl::math::radians(32.f)));
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
    if (!window.create(headless ? "lights-screenshot" : "FuhGLCore - lights demo", args.width, args.height)) {
        std::fprintf(stderr, "lights: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "lights: need GL >= 3.3\n");
        return 2;
    }

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 2.8f, 8.2f};
    cam.pitch = -0.26f;

    std::vector<fuhgl::render::Mesh> meshes;
    const float aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    fuhgl::render::SceneView scene = makeScene(cam, aspect, args.width, args.height, meshes);

    fuhgl::render::ForwardPipeline pipeline;
    if (!pipeline.create()) {
        std::fprintf(stderr, "lights: pipeline create failed\n");
        return 1;
    }

    fuhgl::Texture shotTex(fuhgl::Texture::Target::Tex2D);
    fuhgl::Renderbuffer shotDepth;
    fuhgl::Framebuffer target;
    if (headless) {
        shotTex.bind();
        shotTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, args.width, args.height);
        shotDepth.create();
        shotDepth.bind();
        shotDepth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, args.width, args.height);
        target.create();
        target.setSize(args.width, args.height);
        target.bind();
        target.attachTexture2D(0, shotTex);
        target.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, shotDepth);
        if (!target.isComplete()) {
            std::fprintf(stderr, "lights: target FBO incomplete\n");
            return 1;
        }
        pipeline.setTarget(&target);
    }

    auto renderOnce = [&]() { pipeline.render(scene); };

    if (headless) {
        renderOnce();
        fuhgl::gl::flush();
        target.bind();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "lights: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "lights: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(), args.width, args.height,
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
