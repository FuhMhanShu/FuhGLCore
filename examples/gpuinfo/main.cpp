// GPU info & capabilities demo: queries fuhgl::gl::queryDeviceInfo(), prints a
// human-readable report and renders the key limits as horizontal bars through
// fuhgl::render::ForwardPipeline. Windowed by default; with --screenshot it
// renders one deterministic frame headless (surfaceless EGL) into an FBO and
// writes a PNG — used by the CI docs job for the tutorial screenshot.
//
// Usage: gpuinfo [--screenshot out.png] [--width W] [--height H]

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include "../screenshot/png_writer.hpp"

#include <cmath>
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

struct Bar {
    const char* label;
    double value;
    double ref; // log-scale reference (a "full" bar)
    fuhgl::math::Vec3 color;
};

// One bar per interesting limit; length = 1 + 7 * log10(value)/log10(ref).
std::vector<Bar> makeBars(const fuhgl::gl::DeviceInfo& info) {
    using fuhgl::math::Vec3;
    const fuhgl::gl::Limits& l = info.limits;
    return {
        {"max 2D texture size", (double)l.maxTextureSize2D, 16384.0, {0.95f, 0.45f, 0.30f}},
        {"max 3D texture size", (double)l.maxTextureSize3D, 16384.0, {0.95f, 0.70f, 0.25f}},
        {"max cube texture size", (double)l.maxTextureSizeCube, 16384.0, {0.55f, 0.85f, 0.30f}},
        {"max MSAA samples", (double)l.maxSamples, 16.0, {0.25f, 0.75f, 0.55f}},
        {"max draw buffers (MRT)", (double)l.maxDrawBuffers, 16.0, {0.30f, 0.55f, 0.95f}},
        {"max vertex attribs", (double)l.maxVertexAttribs, 64.0, {0.55f, 0.40f, 0.90f}},
        {"max UBO size [MiB]", l.maxUniformBufferSize / 1048576.0, 64.0, {0.90f, 0.40f, 0.70f}},
        {"max SSBO size [GiB]", l.maxShaderStorageBufferSize / 1073741824.0, 1.0, {0.40f, 0.85f, 0.90f}},
        {"max compute invocations", (double)l.maxComputeWorkGroupInvocations, 1024.0, {0.85f, 0.85f, 0.40f}},
    };
}

double barLength(const Bar& b) {
    const double v = b.value > 0.0 ? b.value : 1.0;
    double t = std::log10(v) / std::log10(b.ref);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return 1.0 + 7.0 * t;
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
    if (!window.create(headless ? "gpuinfo-screenshot" : "FuhGLCore - GPU info",
                       args.width, args.height)) {
        std::fprintf(stderr, "gpuinfo: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gpuinfo: need GL >= 3.3\n");
        return 2;
    }

    const fuhgl::gl::DeviceInfo info = fuhgl::gl::queryDeviceInfo();
    std::printf("%s", info.report().c_str());
    for (const Bar& b : makeBars(info))
        std::printf("  bar %-26s value=%g\n", b.label, b.value);

    // --- capability bars scene --------------------------------------------------
    const std::vector<Bar> bars = makeBars(info);
    const int N = static_cast<int>(bars.size());
    const float rowH = 1.0f, barH = 0.30f, x0 = -7.0f;

    std::vector<fuhgl::render::Mesh> meshes(N);
    fuhgl::render::SceneView scene;
    for (int i = 0; i < N; ++i) {
        const float len = static_cast<float>(barLength(bars[i]));
        const float y = (N - 1 - i) * rowH;
        fuhgl::render::shapes::box(meshes[i], {len / 2.f, barH, 0.4f}, bars[i].color);
        scene.items.push_back({&meshes[i], nullptr, fuhgl::math::translate({x0 + len / 2.f, y, 0.f})});
    }
    scene.lights.push_back({{3.f, 6.f, 10.f}, {1.f, 0.95f, 0.85f}, 30.f});
    scene.lights.push_back({{-8.f, 2.f, 6.f}, {0.5f, 0.65f, 1.f}, 15.f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, (N - 1) * rowH / 2.f, 16.f};
    cam.pitch = 0.f;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);

    fuhgl::render::ForwardPipeline forward;
    if (!forward.create()) { std::fprintf(stderr, "gpuinfo: forward pipeline failed\n"); return 1; }

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
        if (!shotTarget.isComplete()) { std::fprintf(stderr, "gpuinfo: shot FBO incomplete\n"); return 1; }
        forward.setTarget(&shotTarget);
    }

    if (headless) {
        forward.render(scene);
        fuhgl::gl::flush();
        shotTarget.bind();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "gpuinfo: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "gpuinfo: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     args.width, args.height, fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    // windowed: fixed camera, Esc quits
    forward.setTarget(nullptr);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
    });
    while (!window.shouldClose()) {
        window.pollEvents();
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        fuhgl::Framebuffer::unbind();
        forward.render(scene);
        window.swapBuffers();
    }
    return 0;
}
