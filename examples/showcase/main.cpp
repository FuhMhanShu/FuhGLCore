// README showcase (F25): a small landscape rendered with the DeferredRenderer and its
// full effect stack — cascaded shadow maps (F30), SSAO, HDR lighting with a visible sun
// disk, bloom, crepuscular god rays, distance fog, TAA (F32) and SSR (F33), plus a
// gradient sky. CSG solids (rocks, trees, monolith) sit on a textured ground.
// Three looks: --mode day | --mode sunset (sun low in frame -> long shadows + strong god
// rays) | --mode night (dark sky, IBL environment, emissive lamps reflected via SSR).
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

#include <cmath>
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

float hash01(int i, int j, int seed) {
    const float s = std::sin(i * 12.9898f + j * 78.233f + seed * 37.719f) * 43758.5453f;
    return s - std::floor(s);
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);
    const bool sunset = args.mode == "sunset";
    const bool night = args.mode == "night";
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

    // --- renderer with the full effect stack -------------------------------------------
    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 4.2f, 15.5f};
    cam.pitch = -0.20f;

    DeferredRenderer renderer;
    if (!renderer.create(args.width, args.height)) {
        std::fprintf(stderr, "showcase: renderer create failed\n");
        return 1;
    }
    renderer.enableCsmShadows(2048, 4); // F30: cascaded shadow maps
    renderer.setShadowZRange(0.5f, 60.f);
    renderer.setShadowSceneBounds({0.f, 1.f, -2.f}, 18.f);
    renderer.setShadowStrength(1.f);
    renderer.ssaoEnabled = true;
    renderer.ssaoRadius = 0.8f;
    renderer.ssaoStrength = 1.1f;
    renderer.bloomStrength = night ? 1.0f : 0.8f;
    renderer.bloomThreshold = night ? 0.8f : 1.0f;
    renderer.godRaysEnabled = true;
    renderer.rayWeight = sunset ? 0.55f : 0.35f;
    renderer.exposure = 1.05f;
    renderer.taaEnabled = true;  // F32
    renderer.ssrEnabled = true;  // F33
    renderer.ssrStrength = 0.55f;
    renderer.ssrMaxSteps = 48;
    renderer.ssrThickness = 0.5f;
    if (night) {
        renderer.pipeline().skyTop = {0.015f, 0.02f, 0.06f};
        renderer.pipeline().skyBottom = {0.06f, 0.08f, 0.16f};
    } else if (sunset) {
        renderer.pipeline().skyTop = {0.10f, 0.07f, 0.22f};
        renderer.pipeline().skyBottom = {0.95f, 0.42f, 0.16f};
    } else {
        renderer.pipeline().skyTop = {0.16f, 0.40f, 0.85f};
        renderer.pipeline().skyBottom = {0.72f, 0.80f, 0.92f};
    }

    // --- ground: big box with repeated procedural noise (grass) -------------------------
    MeshData groundData = shapes::boxData({16.f, 0.5f, 16.f}, {1.f, 1.f, 1.f});
    for (size_t v = 0; v < groundData.vertexCount(); ++v) {
        float* uv = groundData.vertices.data() + v * MeshData::kFloatsPerVertex + 9;
        uv[0] *= 14.f;
        uv[1] *= 14.f;
    }
    Mesh groundMesh;
    groundMesh.setMeshData(groundData);
    fuhgl::Texture grassTex = procedural::makeTexture(procedural::noise(256, 256, 7u, 5));
    Material grassMat = materials::textured(grassTex, sunset ? fuhgl::math::Vec3{0.38f, 0.30f, 0.16f}
                                                            : fuhgl::math::Vec3{0.30f, 0.50f, 0.22f});

    // --- trees: one CSG mesh (trunk | cone), placed many times ---------------------------
    auto treeSolid = makeCylinder(0.09f, 1.1f) | translated(makeCone(0.75f, 2.0f), {0.f, 2.1f, 0.f});
    Mesh treeMesh = treeSolid->buildMesh(24, {1.f, 1.f, 1.f});
    Material treeMat = materials::matte(sunset ? fuhgl::math::Vec3{0.16f, 0.22f, 0.10f} : fuhgl::math::Vec3{0.14f, 0.38f, 0.16f});

    // --- rocks: two CSG blobs --------------------------------------------------------------
    auto rockSolidA = makeSphere(0.9f) - translated(makeSphere(0.55f), {0.4f, 0.35f, 0.2f});
    auto rockSolidB = makeSphere(0.7f) - translated(makeSphere(0.5f), {-0.3f, 0.3f, -0.25f});
    Mesh rockMeshA = rockSolidA->buildMesh(22, {1.f, 1.f, 1.f});
    Mesh rockMeshB = rockSolidB->buildMesh(22, {1.f, 1.f, 1.f});
    fuhgl::Texture rockTex = procedural::makeTexture(procedural::voronoi(128, 128, 8, 99u,
                                                                  {0.16f, 0.15f, 0.14f}, {0.55f, 0.53f, 0.50f}));
    Material rockMat = materials::textured(rockTex, {1.f, 1.f, 1.f});

    // --- monolith: cube with a spherical bite, dark stone ----------------------------------
    auto monoSolid = makeCube({0.7f, 2.6f, 0.7f}) - makeSphere(0.55f);
    Mesh monoMesh = monoSolid->buildMesh(36, {1.f, 1.f, 1.f});
    fuhgl::Texture stoneTex = procedural::makeTexture(procedural::voronoi(128, 128, 5, 5u,
                                                                   {0.10f, 0.10f, 0.12f}, {0.38f, 0.36f, 0.34f}));
    Material stoneMat = materials::textured(stoneTex, {1.f, 1.f, 1.f});

    // --- scene -----------------------------------------------------------------------------
    SceneView scene;
    scene.camera = &cam;
    scene.width = args.width;
    scene.height = args.height;
    scene.aspect = static_cast<float>(args.width) / static_cast<float>(args.height);
    scene.clearColor = night ? fuhgl::math::Vec3{0.01f, 0.015f, 0.04f}
                     : sunset ? fuhgl::math::Vec3{0.10f, 0.05f, 0.10f}
                              : fuhgl::math::Vec3{0.05f, 0.07f, 0.12f};
    scene.ambient = night ? fuhgl::math::Vec3{0.04f, 0.05f, 0.09f}
                  : sunset ? fuhgl::math::Vec3{0.20f, 0.12f, 0.10f}
                           : fuhgl::math::Vec3{0.16f, 0.17f, 0.20f};
    scene.fogColor = night ? fuhgl::math::Vec3{0.02f, 0.03f, 0.07f}
                   : sunset ? fuhgl::math::Vec3{0.55f, 0.25f, 0.20f}
                            : fuhgl::math::Vec3{0.65f, 0.75f, 0.90f};
    scene.fogDensity = night ? 0.010f : sunset ? 0.012f : 0.008f;
    scene.items.push_back({&groundMesh, &grassMat, fuhgl::math::translate({0.f, -0.5f, 0.f})});
    scene.items.push_back({&monoMesh, &stoneMat, fuhgl::math::translate({-4.5f, 2.6f, -6.f}) * fuhgl::math::rotateY(0.5f)});

    // 12 trees on a ring, deterministic jitter
    for (int i = 0; i < 12; ++i) {
        const float a = hash01(i, 0, 11) * 6.2831853f;
        const float r = 4.5f + hash01(i, 0, 12) * 8.5f;
        const float s = 0.8f + hash01(i, 0, 13) * 0.7f;
        scene.items.push_back({&treeMesh, &treeMat,
                               fuhgl::math::translate({std::cos(a) * r, 0.f, std::sin(a) * r - 2.f}) *
                               fuhgl::math::rotateY(hash01(i, 0, 14) * 6.2831853f) * fuhgl::math::scale({s, s, s})});
    }
    // 8 rocks
    for (int i = 0; i < 8; ++i) {
        const float a = hash01(i, 1, 15) * 6.2831853f;
        const float r = 2.5f + hash01(i, 1, 16) * 10.f;
        const float s = 0.5f + hash01(i, 1, 17) * 1.1f;
        Mesh& rm = (i % 2 == 0) ? rockMeshA : rockMeshB;
        scene.items.push_back({&rm, &rockMat,
                               fuhgl::math::translate({std::cos(a) * r, 0.15f * s, std::sin(a) * r - 1.f}) *
                               fuhgl::math::rotateY(hash01(i, 1, 18) * 6.2831853f) * fuhgl::math::scale({s, s * 0.8f, s})});
    }

    // sun: day = high warm light; sunset = low in front of the camera (god rays in frame);
    // night = dim blue "moon"
    const Light sun = night ? Light::directional({-0.3f, -1.f, -0.2f}, {0.35f, 0.45f, 0.70f}, 0.9f)
                    : sunset ? Light::directional({0.25f, -0.10f, 1.f}, {1.f, 0.45f, 0.20f}, 3.0f)
                             : Light::directional({-0.4f, -1.f, -0.25f}, {1.f, 0.96f, 0.88f}, 2.6f);
    scene.lights.push_back(sun);
    scene.lights.push_back(Light::point({-6.f, 5.f, 4.f},
                                        night ? fuhgl::math::Vec3{0.9f, 0.55f, 0.25f}
                                        : sunset ? fuhgl::math::Vec3{0.5f, 0.25f, 0.15f}
                                                 : fuhgl::math::Vec3{0.45f, 0.55f, 0.85f}, 10.f, 18.f));

    // night: IBL environment baked from a dark gradient cubemap + emissive lamp posts
    // whose reflections show up in the ground via SSR
    fuhgl::render::Environment env;
    Mesh lampMesh;           // must outlive the scene: items store &lampMesh
    Material lampMat;
    if (night) {
        fuhgl::Skybox nightSky;
        nightSky.create();
        nightSky.makeGradient({0.01f, 0.02f, 0.07f}, {0.10f, 0.12f, 0.22f}, 64);
        if (!env.createFromCubemap(nightSky.texture())) {
            std::fprintf(stderr, "showcase: night IBL bake failed\n");
            return 1;
        }
        env.intensity = 1.0f;
        scene.env = &env;
        shapes::box(lampMesh, {0.12f, 0.9f, 0.12f}, {1.f, 1.f, 1.f});
        lampMat = materials::emissive({1.f, 0.72f, 0.35f}, 6.f);
        const fuhgl::math::Vec3 lampPos[3] = {{-3.f, 0.f, 2.f}, {3.5f, 0.f, -3.f}, {0.5f, 0.f, -7.f}};
        for (const auto& p : lampPos) {
            scene.items.push_back({&lampMesh, &lampMat, fuhgl::math::translate(p)});
            scene.lights.push_back(Light::point({p.x, 1.8f, p.z}, {1.f, 0.7f, 0.4f}, 14.f, 16.f));
        }
    }

    TextRenderer text;
    if (!text.create()) {
        std::fprintf(stderr, "showcase: TextRenderer create failed\n");
        return 1;
    }
    auto drawHud = [&](float fpsValue) {
        char line[200];
        text.begin(scene.width, scene.height);
        text.draw(night ? "FuhGLCore showcase - night (IBL + SSR)"
                : sunset ? "FuhGLCore showcase - sunset (god rays)"
                         : "FuhGLCore showcase - day",
                  14.f, 12.f, 2.f, {1.f, 1.f, 1.f, 1.f});
        std::snprintf(line, sizeof(line),
                      "deferred renderer: CSM + SSAO + HDR + bloom + god rays + TAA + SSR + fog | FPS: %.1f",
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
                     args.mode.c_str(), args.width, args.height,
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
