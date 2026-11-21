// Headless smoke test for solids/CSG + preset materials: builds a CSG body
// (cube minus sphere) via marching tetrahedra, textures it with a procedural
// checker material and renders it through DeferredPipeline; verifies a lit,
// non-sky center pixel and a sky corner. Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_solids", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_solids: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_solids: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 256, H = 256;
    fuhgl::render::DeferredPipeline pipeline;
    if (!pipeline.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_solids: pipeline create failed\n");
        return 1;
    }

    // CSG body: cube with a spherical bite, meshed by marching tetrahedra
    auto body = fuhgl::render::makeCube({1.f, 1.f, 1.f}) - fuhgl::render::makeSphere(0.8f);
    fuhgl::render::Mesh bodyMesh = body->buildMesh(48, {1.f, 1.f, 1.f});
    if (bodyMesh.indexCount() < 100) {
        std::fprintf(stderr, "gl_smoke_solids: FAIL — CSG mesh too small (%d indices)\n", bodyMesh.indexCount());
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_solids: CSG mesh: %d verts, %d indices\n",
                 bodyMesh.vertexCount(), bodyMesh.indexCount());

    // procedural checker texture + predefined textured material
    auto checkerImg = fuhgl::render::procedural::checker(64, 64, 8, {0.95f, 0.45f, 0.2f}, {0.2f, 0.3f, 0.9f});
    fuhgl::Texture checkerTex = fuhgl::render::procedural::makeTexture(checkerImg);
    fuhgl::render::Material checkerMat = fuhgl::render::materials::textured(checkerTex);

    // a matte torus next to it
    auto torus = fuhgl::render::makeTorus(0.55f, 0.18f);
    fuhgl::render::Mesh torusMesh = torus->buildMesh(32, {0.3f, 0.8f, 0.4f});
    fuhgl::render::Material matteGreen = fuhgl::render::materials::matte({0.3f, 0.8f, 0.4f});

    // F23: sphere meshed with spherical UVs, same textured material (UV path coverage)
    auto ball = fuhgl::render::makeSphere(0.6f);
    fuhgl::render::Mesh ballMesh = ball->buildMesh(32, {1.f, 1.f, 1.f}, fuhgl::render::SolidUv::Spherical);

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 1.2f, 5.f};
    cam.pitch = -0.12f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.items.push_back({&bodyMesh, &checkerMat, fuhgl::math::translate({-0.9f, 0.f, 0.f})});
    scene.items.push_back({&torusMesh, &matteGreen, fuhgl::math::translate({1.1f, 0.2f, 0.f})});
    scene.items.push_back({&ballMesh, &checkerMat, fuhgl::math::translate({0.1f, 0.95f, -0.1f})});
    scene.lights.push_back({{3.f, 3.f, 4.f}, {1.f, 0.9f, 0.7f}, 25.f});
    scene.lights.push_back({{-4.f, 2.f, -2.f}, {0.4f, 0.6f, 1.f}, 18.f});

    pipeline.render(scene);
    fuhgl::gl::flush();
    if (fuhgl::gl::lastError() != fuhgl::gl::Error::NoError) {
        std::fprintf(stderr, "gl_smoke_solids: FAIL — GL error after render\n");
        return 1;
    }

    pipeline.output().bind();
    unsigned char center[4] = {0, 0, 0, 0}, corner[4] = {0, 0, 0, 0};
    fuhgl::gl::readPixels(W / 2, H / 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, center);
    fuhgl::gl::readPixels(2, 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, corner);
    std::fprintf(stderr, "gl_smoke_solids: center=(%d,%d,%d,%d) corner=(%d,%d,%d,%d)\n",
                 center[0], center[1], center[2], center[3], corner[0], corner[1], corner[2], corner[3]);

    const bool centerLit = center[3] == 255 && (center[0] + center[1] + center[2]) > 60;
    const bool cornerSky = corner[2] > corner[0];
    if (!centerLit || !cornerSky) {
        std::fprintf(stderr, "gl_smoke_solids: FAIL\n");
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_solids: OK\n");
    return 0;
}
