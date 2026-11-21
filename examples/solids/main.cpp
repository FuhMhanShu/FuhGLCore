// Solids & CSG demo: builds bodies analytically (cube/sphere/cylinder/torus/cone),
// combines them with CSG operators (| & -), meshes them via marching tetrahedra and
// renders the scene through DeferredPipeline with predefined materials
// (matte / textured with a procedural voronoi / emissive). Windowed by default;
// with --screenshot it renders one deterministic frame headless (surfaceless EGL)
// into an FBO and writes a PNG — used by the CI docs job for the tutorial screenshot.
//
// Usage: solids [--screenshot out.png] [--width W] [--height H]

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
    if (!window.create(headless ? "solids-screenshot" : "FuhGLCore - solids & CSG",
                       args.width, args.height)) {
        std::fprintf(stderr, "solids: no window/GL context available\n");
        return headless ? 2 : 1;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "solids: need GL >= 3.3\n");
        return 2;
    }

    using namespace fuhgl::render;

    // --- bodies: primitives + CSG ---------------------------------------------------
    // left: cube with a spherical bite, textured with a procedural voronoi
    auto part = makeCube({1.0f, 1.0f, 1.0f}) - makeSphere(0.75f);
    Mesh partMesh = part->buildMesh(48, {1.f, 1.f, 1.f});
    std::fprintf(stderr, "solids: CSG part (cube-sphere): %d verts, %d indices\n",
                 partMesh.vertexCount(), partMesh.indexCount());

    // bounding volumes of the CSG part (box / sphere / low-poly octahedron) — for culling etc.
    auto bb = makeBoundingBox(*part);
    auto bs = makeBoundingSphere(*part);
    auto bo = makeBoundingOctahedron(*part);
    std::fprintf(stderr, "solids: bounding box aabb=[%.2f %.2f %.2f]..[%.2f %.2f %.2f]\n",
                 bb->aabbMin().x, bb->aabbMin().y, bb->aabbMin().z,
                 bb->aabbMax().x, bb->aabbMax().y, bb->aabbMax().z);
    std::fprintf(stderr, "solids: bounding sphere r=%.3f, octahedron verts=%zu\n",
                 (bs->aabbMax().x - bs->aabbMin().x) * 0.5f, bo->buildMeshData(8).vertexCount());
    // frustum culling example: render::intersects(frustum, *part)
    (void)bo;

    // middle: torus with a cylindrical hole (union of torus + nothing, difference of cylinder)
    auto ring = makeTorus(0.55f, 0.2f) - makeCylinder(0.18f, 1.6f);
    Mesh ringMesh = ring->buildMesh(40, {0.35f, 0.75f, 0.45f});

    // right: cone stacked on a cylinder pedestal (union of translated parts)
    auto pedestal = makeCylinder(0.45f, 0.25f) | translated(makeCone(0.45f, 0.9f), {0.f, 0.55f, 0.f});
    Mesh pedestalMesh = pedestal->buildMesh(40, {0.85f, 0.6f, 0.25f});

    // emissive marker sphere floating above
    auto marker = makeSphere(0.22f);
    Mesh markerMesh = marker->buildMesh(24, {0.f, 0.f, 0.f});

    // --- procedural texture + predefined materials ----------------------------------
    auto voronoiImg = procedural::voronoi(128, 128, 6, 1337u,
                                          {0.12f, 0.14f, 0.22f}, {0.95f, 0.75f, 0.35f});
    fuhgl::Texture voronoiTex = procedural::makeTexture(voronoiImg);

    Material partMat = materials::textured(voronoiTex, {1.f, 1.f, 1.f});
    Material ringMat = materials::matte({0.35f, 0.75f, 0.45f});
    Material pedestalMat = materials::matte({0.85f, 0.6f, 0.25f});
    Material markerMat = materials::emissive({1.f, 0.35f, 0.15f}, 2.5f);

    // --- scene -----------------------------------------------------------------------
    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.4f, 5.6f};
    cam.pitch = -0.10f;

    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    scene.items.push_back({&partMesh, &partMat, fuhgl::math::translate({-1.7f, -0.2f, 0.f})});
    scene.items.push_back({&ringMesh, &ringMat, fuhgl::math::translate({0.f, 0.35f, -0.4f})});
    scene.items.push_back({&pedestalMesh, &pedestalMat, fuhgl::math::translate({1.7f, -0.7f, 0.f})});
    scene.items.push_back({&markerMesh, &markerMat, fuhgl::math::translate({0.4f, 1.5f, -0.4f})});
    scene.lights.push_back({{3.f, 4.f, 5.f}, {1.f, 0.95f, 0.85f}, 30.f});
    scene.lights.push_back({{-5.f, 2.f, -3.f}, {0.45f, 0.6f, 1.f}, 20.f});

    DeferredPipeline pipeline;
    if (!pipeline.create(args.width, args.height)) {
        std::fprintf(stderr, "solids: deferred pipeline failed\n");
        return 1;
    }

    auto renderOnce = [&]() { pipeline.render(scene); };

    if (headless) {
        renderOnce();
        fuhgl::gl::flush();
        pipeline.output().bind();
        std::vector<unsigned char> px(static_cast<size_t>(args.width) * args.height * 4);
        fuhgl::gl::readPixels(0, 0, args.width, args.height, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        std::vector<unsigned char> flipped(px.size());
        const size_t stride = static_cast<size_t>(args.width) * 4;
        for (int y = 0; y < args.height; ++y)
            std::memcpy(flipped.data() + static_cast<size_t>(y) * stride,
                        px.data() + static_cast<size_t>(args.height - 1 - y) * stride, stride);
        if (!screenshot::writePng(args.screenshot.c_str(), flipped.data(), args.width, args.height)) {
            std::fprintf(stderr, "solids: failed to write %s\n", args.screenshot.c_str());
            return 1;
        }
        std::fprintf(stderr, "solids: wrote %s (%dx%d, GL %d.%d)\n", args.screenshot.c_str(),
                     args.width, args.height, fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    // windowed loop
    cam.attach(window);
    window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
        if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
        cam.onKey(key, action);
    });
    double lastTime = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        cam.update(static_cast<float>(now - lastTime));
        lastTime = now;
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        if (scene.width != pipeline.width() || scene.height != pipeline.height())
            pipeline.resize(scene.width, scene.height);
        pipeline.render(scene);
        fuhgl::Framebuffer::blitToDefault(pipeline.output(), scene.width, scene.height);
        window.swapBuffers();
    }
    return 0;
}
