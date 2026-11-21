// Solids & CSG demo: builds bodies analytically (cube/sphere/cylinder/torus/cone),
// combines them with CSG operators (| & -), meshes them via marching tetrahedra and
// renders the scene through DeferredPipeline with predefined materials
// (matte / textured with a procedural voronoi / emissive).
//
// Modes: default = full scene (windowed, WASD+mouse, Esc quits); --csg union|difference|
// intersection|bounds renders a CSG close-up instead; --wireframe overlays every solid
// as a triangle-edge wireframe plus its AABB as a box wireframe (render::Wireframe).
// With --screenshot one deterministic frame is rendered headless (surfaceless EGL) into
// an FBO and written as PNG — the CI docs job uses this for the tutorial screenshots
// (scene + the four CSG shots + the wireframe shot).
//
// Usage: solids [--screenshot out.png] [--csg scene|union|difference|intersection|bounds]
//               [--wireframe] [--width W] [--height H]

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
    std::string csg = "scene";
    bool wireframe = false;
    int width = 800, height = 600;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) a.screenshot = argv[++i];
        else if (std::strcmp(argv[i], "--csg") == 0 && i + 1 < argc) a.csg = argv[++i];
        else if (std::strcmp(argv[i], "--wireframe") == 0) a.wireframe = true;
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
    auto part = makeCube({1.0f, 1.0f, 1.0f}) - makeSphere(0.75f);
    Mesh partMesh = part->buildMesh(48, {1.f, 1.f, 1.f});
    std::fprintf(stderr, "solids: CSG part (cube-sphere): %d verts, %d indices\n",
                 partMesh.vertexCount(), partMesh.indexCount());

    auto ring = makeTorus(0.55f, 0.2f) - makeCylinder(0.18f, 1.6f);
    Mesh ringMesh = ring->buildMesh(40, {0.35f, 0.75f, 0.45f});

    auto pedestal = makeCylinder(0.45f, 0.25f) | translated(makeCone(0.45f, 0.9f), {0.f, 0.55f, 0.f});
    Mesh pedestalMesh = pedestal->buildMesh(40, {0.85f, 0.6f, 0.25f});

    auto marker = makeSphere(0.22f);
    Mesh markerMesh = marker->buildMesh(24, {0.f, 0.f, 0.f});

    // bounding volumes of the CSG part (box / sphere / low-poly octahedron)
    auto bb = makeBoundingBox(*part);
    auto bs = makeBoundingSphere(*part);
    auto bo = makeBoundingOctahedron(*part);
    std::fprintf(stderr, "solids: bounding box aabb=[%.2f %.2f %.2f]..[%.2f %.2f %.2f]\n",
                 bb->aabbMin().x, bb->aabbMin().y, bb->aabbMin().z,
                 bb->aabbMax().x, bb->aabbMax().y, bb->aabbMax().z);
    std::fprintf(stderr, "solids: bounding sphere r=%.3f, octahedron verts=%zu\n",
                 (bs->aabbMax().x - bs->aabbMin().x) * 0.5f, bo->buildMeshData(8).vertexCount());

    // CSG close-up bodies for the tutorial shots
    auto uni = makeCube({0.85f, 0.85f, 0.85f}) | makeSphere(0.8f);
    auto inter = makeSphere(0.95f) & makeCube({0.8f, 0.8f, 0.8f});
    Mesh uniMesh = uni->buildMesh(48, {0.95f, 0.55f, 0.25f});
    Mesh interMesh = inter->buildMesh(48, {0.45f, 0.65f, 0.95f});
    Mesh bbMesh = bb->buildMesh(16, {0.9f, 0.9f, 0.95f});
    Mesh bsMesh = bs->buildMesh(32, {0.9f, 0.75f, 0.3f});
    Mesh boMesh = bo->buildMesh(16, {0.4f, 0.85f, 0.9f});

    // --- procedural texture + predefined materials ----------------------------------
    auto voronoiImg = procedural::voronoi(128, 128, 6, 1337u,
                                          {0.12f, 0.14f, 0.22f}, {0.95f, 0.75f, 0.35f});
    fuhgl::Texture voronoiTex = procedural::makeTexture(voronoiImg);

    Material partMat = materials::textured(voronoiTex, {1.f, 1.f, 1.f});
    Material ringMat = materials::matte({0.35f, 0.75f, 0.45f});
    Material pedestalMat = materials::matte({0.85f, 0.6f, 0.25f});
    Material markerMat = materials::emissive({1.f, 0.35f, 0.15f}, 2.5f);
    Material uniMat = materials::matte({0.95f, 0.55f, 0.25f});
    Material interMat = materials::matte({0.45f, 0.65f, 0.95f});
    Material boxMat = materials::matte({0.9f, 0.9f, 0.95f});
    Material sphMat = materials::matte({0.9f, 0.75f, 0.3f});
    Material octMat = materials::matte({0.4f, 0.85f, 0.9f});

    // --- scene -----------------------------------------------------------------------
    fuhgl::FirstPersonCamera cam;
    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    scene.lights.push_back({{3.f, 4.f, 5.f}, {1.f, 0.95f, 0.85f}, 30.f});
    scene.lights.push_back({{-5.f, 2.f, -3.f}, {0.45f, 0.6f, 1.f}, 20.f});

    if (args.csg == "union") {
        cam.position = {0.f, 0.9f, 3.4f};
        cam.pitch = -0.10f;
        scene.items.push_back({&uniMesh, &uniMat, fuhgl::math::Mat4{}});
    } else if (args.csg == "difference") {
        cam.position = {0.f, 0.9f, 3.4f};
        cam.pitch = -0.10f;
        scene.items.push_back({&partMesh, &partMat, fuhgl::math::Mat4{}});
    } else if (args.csg == "intersection") {
        cam.position = {0.f, 0.9f, 3.4f};
        cam.pitch = -0.10f;
        scene.items.push_back({&interMesh, &interMat, fuhgl::math::Mat4{}});
    } else if (args.csg == "bounds") {
        // CSG part + its three bounding volumes in a row (part | octahedron | sphere | box)
        cam.position = {0.f, 0.7f, 7.0f};
        cam.pitch = -0.07f;
        scene.items.push_back({&partMesh, &partMat, fuhgl::math::translate({-3.6f, 0.f, 0.f})});
        scene.items.push_back({&boMesh, &octMat, fuhgl::math::translate({-1.2f, 0.f, 0.f})});
        scene.items.push_back({&bsMesh, &sphMat, fuhgl::math::translate({1.2f, 0.f, 0.f})});
        scene.items.push_back({&bbMesh, &boxMat, fuhgl::math::translate({3.6f, 0.f, 0.f})});
    } else { // "scene": full demo scene
        cam.position = {0.f, 1.4f, 5.6f};
        cam.pitch = -0.10f;
        scene.items.push_back({&partMesh, &partMat, fuhgl::math::translate({-1.7f, -0.2f, 0.f})});
        scene.items.push_back({&ringMesh, &ringMat, fuhgl::math::translate({0.f, 0.35f, -0.4f})});
        scene.items.push_back({&pedestalMesh, &pedestalMat, fuhgl::math::translate({1.7f, -0.7f, 0.f})});
        scene.items.push_back({&markerMesh, &markerMat, fuhgl::math::translate({0.4f, 1.5f, -0.4f})});
    }

    DeferredPipeline pipeline;
    if (!pipeline.create(args.width, args.height)) {
        std::fprintf(stderr, "solids: deferred pipeline failed\n");
        return 1;
    }

    Wireframe wire;
    if (args.wireframe && !wire.create()) {
        std::fprintf(stderr, "solids: wireframe create failed\n");
        return 1;
    }
    auto queueWireframe = [&]() {
        wire.clear();
        if (args.csg == "scene") {
            wire.addSolid(*part, 24, fuhgl::math::translate({-1.7f, -0.2f, 0.f}), {0.95f, 0.95f, 1.f});
            wire.addSolid(*ring, 20, fuhgl::math::translate({0.f, 0.35f, -0.4f}), {0.4f, 0.95f, 0.5f});
            wire.addSolid(*pedestal, 20, fuhgl::math::translate({1.7f, -0.7f, 0.f}), {1.f, 0.75f, 0.35f});
            wire.addBoundingBox(*part, fuhgl::math::translate({-1.7f, -0.2f, 0.f}), {1.f, 0.9f, 0.2f});
            wire.addBoundingBox(*ring, fuhgl::math::translate({0.f, 0.35f, -0.4f}), {0.3f, 0.8f, 1.f});
            wire.addBoundingBox(*pedestal, fuhgl::math::translate({1.7f, -0.7f, 0.f}), {1.f, 0.4f, 0.6f});
        } else {
            const Solid* s = args.csg == "union" ? uni.get()
                             : args.csg == "intersection" ? inter.get() : part.get();
            wire.addSolid(*s, 24, fuhgl::math::Mat4{}, {0.95f, 0.95f, 1.f});
            wire.addBoundingBox(*s, fuhgl::math::Mat4{}, {1.f, 0.9f, 0.2f});
        }
    };

    if (headless) {
        pipeline.render(scene);
        if (args.wireframe) {
            queueWireframe();
            pipeline.output().bind();
            wire.render(cam, args.width, args.height);
        }
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
        std::fprintf(stderr, "solids: wrote %s (csg=%s, %dx%d, GL %d.%d)\n", args.screenshot.c_str(), args.csg.c_str(),
                     args.width, args.height, fuhgl::gl::version().major, fuhgl::gl::version().minor);
        return 0;
    }

    // windowed loop (full scene only; CSG modes use a fixed close-up camera)
    if (args.csg == "scene") {
        cam.attach(window);
        window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
            if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
            cam.onKey(key, action);
        });
    } else {
        window.setKeyCallback([&](fuhgl::Key key, fuhgl::Action action) {
            if (action == fuhgl::Action::Press && key == fuhgl::Key::Escape) window.setShouldClose(true);
        });
    }
    double lastTime = window.time();
    while (!window.shouldClose()) {
        window.pollEvents();
        const double now = window.time();
        if (args.csg == "scene") cam.update(static_cast<float>(now - lastTime));
        lastTime = now;
        scene.width = window.framebufferWidth();
        scene.height = window.framebufferHeight();
        scene.aspect = static_cast<float>(scene.width) / static_cast<float>(scene.height);
        if (scene.width != pipeline.width() || scene.height != pipeline.height())
            pipeline.resize(scene.width, scene.height);
        pipeline.render(scene);
        if (args.wireframe) {
            queueWireframe();
            pipeline.output().bind();
            wire.render(cam, scene.width, scene.height);
            fuhgl::Framebuffer::blitToDefault(pipeline.output(), scene.width, scene.height);
        } else {
            fuhgl::Framebuffer::blitToDefault(pipeline.output(), scene.width, scene.height);
        }
        window.swapBuffers();
    }
    return 0;
}
