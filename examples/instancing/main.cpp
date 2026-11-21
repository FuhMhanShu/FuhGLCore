// Instancing demo (F24): hundreds of bodies rendered with ONE draw call per mesh.
// A grid of cones ("trees") + scattered spheres ("rocks") share one Mesh each and
// get per-instance model matrices via Mesh::setInstanceData() (divisor-1 mat4
// attribute, locations 4..7). The windowed loop animates a wave through
// Mesh::updateInstanceData() every frame. Rendered through DeferredPipeline.
//
// With --screenshot one deterministic frame is rendered headless (surfaceless EGL)
// into the pipeline output and written as PNG (CI docs job: tutorial-instancing.png).
//
// Usage: instancing [--screenshot out.png] [--width W] [--height H]

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

// deterministic pseudo-random in [0,1) from integer coords + seed
float hash01(int i, int j, int seed) {
    const float s = std::sin(i * 12.9898f + j * 78.233f + seed * 37.719f) * 43758.5453f;
    return s - std::floor(s);
}

constexpr int kGridN = 18;      // trees per side
constexpr float kSpacing = 1.6f;
constexpr int kRockCount = 96;

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
    if (!window.create(headless ? "instancing-screenshot" : "FuhGLCore - instancing",
                       args.width, args.height)) {
        std::fprintf(stderr, "instancing: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "instancing: need GL >= 3.3\n");
        return 2;
    }

    using namespace fuhgl::render;

    // --- meshes (built once, drawn instanced) -----------------------------------------
    Mesh groundMesh;
    shapes::box(groundMesh, {16.f, 0.25f, 16.f}, {1.f, 1.f, 1.f});
    Material groundMat = materials::matte({0.16f, 0.24f, 0.13f});

    auto tree = makeCone(0.42f, 1.9f);
    Mesh treeMesh = tree->buildMesh(24, {1.f, 1.f, 1.f});
    Material treeMat = materials::matte({0.30f, 0.62f, 0.25f});

    auto rock = makeSphere(0.5f);
    Mesh rockMesh = rock->buildMesh(20, {1.f, 1.f, 1.f});
    Material rockMat = materials::matte({0.55f, 0.55f, 0.60f});

    // --- instance model matrices --------------------------------------------------------
    const int treeCount = kGridN * kGridN;
    std::vector<fuhgl::math::Mat4> treeModels(treeCount);
    std::vector<fuhgl::math::Mat4> rockModels(kRockCount);
    for (int j = 0; j < kGridN; ++j) {
        for (int i = 0; i < kGridN; ++i) {
            const int idx = j * kGridN + i;
            const float x = (i - (kGridN - 1) * 0.5f) * kSpacing + (hash01(i, j, 1) - 0.5f) * 0.7f;
            const float z = (j - (kGridN - 1) * 0.5f) * kSpacing + (hash01(i, j, 2) - 0.5f) * 0.7f;
            const float s = 0.75f + hash01(i, j, 3) * 0.6f;
            const float rot = hash01(i, j, 4) * 6.2831853f;
            treeModels[idx] = fuhgl::math::translate({x, 0.f, z}) * fuhgl::math::rotateY(rot) *
                              fuhgl::math::scale({s, s, s});
        }
    }
    for (int i = 0; i < kRockCount; ++i) {
        const float a = hash01(i, 0, 5) * 6.2831853f;
        const float r = 3.f + hash01(i, 0, 6) * 11.f;
        const float s = 0.4f + hash01(i, 0, 7) * 0.8f;
        rockModels[i] = fuhgl::math::translate({std::cos(a) * r, 0.30f * s, std::sin(a) * r}) *
                        fuhgl::math::rotateY(hash01(i, 0, 8) * 6.2831853f) *
                        fuhgl::math::scale({s, s * (0.7f + hash01(i, 0, 9) * 0.5f), s});
    }
    // dynamic usage: the windowed loop re-uploads a wave animation every frame
    treeMesh.setInstanceData(treeModels.data(), treeCount, fuhgl::Buffer::Usage::DynamicDraw);
    rockMesh.setInstanceData(rockModels.data(), kRockCount);
    std::fprintf(stderr, "instancing: %d trees + %d rocks = 2 instanced draw calls (meshes: %d/%d verts)\n",
                 treeCount, kRockCount, treeMesh.vertexCount(), rockMesh.vertexCount());

    auto wave = [&](float t) {
        for (int j = 0; j < kGridN; ++j) {
            for (int i = 0; i < kGridN; ++i) {
                const int idx = j * kGridN + i;
                const float x = (i - (kGridN - 1) * 0.5f) * kSpacing;
                const float z = (j - (kGridN - 1) * 0.5f) * kSpacing;
                const float s = 0.75f + hash01(i, j, 3) * 0.6f;
                const float y = 0.95f * s + 0.22f * std::sin(0.9f * x + 0.7f * z + t);
                const float rot = hash01(i, j, 4) * 6.2831853f;
                const float jx = (hash01(i, j, 1) - 0.5f) * 0.7f;
                const float jz = (hash01(i, j, 2) - 0.5f) * 0.7f;
                treeModels[idx] = fuhgl::math::translate({x + jx, y, z + jz}) * fuhgl::math::rotateY(rot) *
                                  fuhgl::math::scale({s, s, s});
            }
        }
        treeMesh.updateInstanceData(treeModels.data(), treeCount);
    };

    // --- scene ---------------------------------------------------------------------------
    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 9.5f, 19.f};
    cam.pitch = -0.42f;

    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    scene.fogColor = {0.05f, 0.06f, 0.10f};
    scene.fogDensity = 0.010f;
    scene.lights.push_back({{8.f, 12.f, 6.f}, {1.f, 0.93f, 0.80f}, 32.f});
    scene.lights.push_back({{-9.f, 5.f, -7.f}, {0.40f, 0.55f, 1.f}, 18.f});
    scene.items.push_back({&groundMesh, &groundMat, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&treeMesh, &treeMat, fuhgl::math::Mat4{}});
    scene.items.push_back({&rockMesh, &rockMat, fuhgl::math::Mat4{}});

    DeferredPipeline pipeline;
    if (!pipeline.create(args.width, args.height)) {
        std::fprintf(stderr, "instancing: deferred pipeline failed\n");
        return 1;
    }

    TextRenderer text;
    if (!text.create()) {
        std::fprintf(stderr, "instancing: TextRenderer create failed\n");
        return 1;
    }
    auto drawHud = [&](float fpsValue) {
        char line[160];
        text.begin(scene.width, scene.height);
        text.draw("FuhGLCore - instancing", 16.f, 14.f, 2.f, {1.f, 1.f, 1.f, 1.f});
        std::snprintf(line, sizeof(line), "%d trees + %d rocks = 2 instanced draw calls (1 mesh each)",
                      treeCount, kRockCount);
        text.draw(line, 16.f, 44.f, 1.f, {0.45f, 0.95f, 0.45f, 1.f});
        std::snprintf(line, sizeof(line), "Mesh::setInstanceData() + updateInstanceData() wave | FPS: %.1f",
                      fpsValue);
        text.draw(line, 16.f, 60.f, 1.f, {0.75f, 0.75f, 0.8f, 1.f});
        text.end();
    };

    if (headless) {
        wave(0.7f); // deterministic pose for the screenshot
        pipeline.render(scene);
        fuhgl::gl::flush();
        pipeline.output().bind();
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
            std::fprintf(stderr, "instancing: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "instancing: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     args.width, args.height, fuhgl::gl::version().major, fuhgl::gl::version().minor);
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
        wave(static_cast<float>(now));
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        if (scene.width != pipeline.width() || scene.height != pipeline.height())
            pipeline.resize(scene.width, scene.height);
        pipeline.render(scene);
        pipeline.output().bind();
        drawHud(fps.fps());
        fuhgl::Framebuffer::blitToDefault(pipeline.output(), scene.width, scene.height);
        window.swapBuffers();
    }
    return 0;
}
