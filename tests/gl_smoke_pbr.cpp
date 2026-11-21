// Headless smoke test for F28 (PBR metallic-roughness) + F29 (IBL environment):
// forward pass: metal vs dielectric differ, IBL env brightens the ambient term;
// deferred pass: 5-attachment G-buffer (metallic/roughness/ao) lights a PBR box.
// Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_pbr", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_pbr: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_pbr: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 128, H = 128;

    // --- source cubemap: uniform gray 0.5 on all faces -----------------------------
    fuhgl::Texture cube(fuhgl::Texture::Target::Cube);
    cube.bind();
    std::vector<unsigned char> facePx(32 * 32 * 3, 128);
    for (int f = 0; f < 6; ++f)
        cube.setImageCubeFace(static_cast<fuhgl::Texture::CubeFace>(fuhgl::gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X + f),
                              0, fuhgl::gl::GL_RGB8, 32, fuhgl::gl::GL_RGB, fuhgl::gl::GL_UNSIGNED_BYTE,
                              facePx.data());

    fuhgl::render::Environment env;
    if (!env.createFromCubemap(cube, 16, 32, 64)) {
        std::fprintf(stderr, "gl_smoke_pbr: environment bake failed\n");
        return 1;
    }
    expect(env.prefilterMaxLod() > 0, "prefilter cubemap has a mip chain");
    expect(env.irradianceMap().id() != 0 && env.prefilterMap().id() != 0 && env.brdfLut().id() != 0,
           "IBL textures uploaded");

    fuhgl::render::ForwardPipeline pipeline;
    if (!pipeline.create()) {
        std::fprintf(stderr, "gl_smoke_pbr: pipeline create failed\n");
        return 1;
    }

    fuhgl::Texture shotTex(fuhgl::Texture::Target::Tex2D);
    shotTex.bind();
    shotTex.texStorage2D(1, fuhgl::gl::GL_RGBA8, W, H);
    fuhgl::Renderbuffer depth;
    depth.create();
    depth.bind();
    depth.storage(fuhgl::gl::GL_DEPTH_COMPONENT24, W, H);
    fuhgl::Framebuffer target;
    target.create();
    target.setSize(W, H);
    target.bind();
    target.attachTexture2D(0, shotTex);
    target.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depth);
    if (!target.isComplete()) {
        std::fprintf(stderr, "gl_smoke_pbr: target FBO incomplete\n");
        return 1;
    }
    pipeline.setTarget(&target);

    fuhgl::render::Mesh box;
    fuhgl::render::shapes::box(box, {1.f, 1.f, 1.f}, {0.8f, 0.8f, 0.8f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 0.5f, 6.f};

    const fuhgl::render::Light sun = fuhgl::render::Light::directional({0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, 0.5f);

    auto renderCenter = [&](const fuhgl::render::Material* mat, const fuhgl::render::Environment* e) {
        fuhgl::render::SceneView scene;
        scene.camera = &cam;
        scene.width = W;
        scene.height = H;
        scene.aspect = 1.f;
        scene.clearColor = {0.f, 0.f, 0.f};
        scene.ambient = {0.06f, 0.06f, 0.06f};
        scene.items.push_back({&box, mat, fuhgl::math::Mat4{}});
        scene.lights.push_back(sun);
        scene.env = e;
        pipeline.render(scene);
        fuhgl::gl::flush();
        unsigned char px[4] = {0, 0, 0, 0};
        target.bind();
        fuhgl::gl::readPixels(W / 2, H / 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
        return px[0];
    };

    fuhgl::render::Material dielectric = fuhgl::render::materials::pbr({0.8f, 0.8f, 0.8f}, 0.f, 0.6f);
    fuhgl::render::Material metal = fuhgl::render::materials::pbr({0.8f, 0.8f, 0.8f}, 1.f, 0.15f);
    fuhgl::render::Material roughMetal = fuhgl::render::materials::pbr({0.8f, 0.8f, 0.8f}, 1.f, 1.f);

    const unsigned char cDielectric = renderCenter(&dielectric, nullptr);
    const unsigned char cMetal = renderCenter(&metal, nullptr);
    const unsigned char cRoughMetal = renderCenter(&roughMetal, nullptr);
    const unsigned char cDielectricEnv = renderCenter(&dielectric, &env);
    std::fprintf(stderr, "gl_smoke_pbr: dielectric=%d metal=%d roughMetal=%d dielectricEnv=%d\n",
                 cDielectric, cMetal, cRoughMetal, cDielectricEnv);

    expect(cDielectric > 60, "PBR dielectric is lit by the directional light");
    expect(std::abs(int(cMetal) - int(cDielectric)) > 8, "metal differs from dielectric under the same light");
    expect(std::abs(int(cRoughMetal) - int(cMetal)) > 8, "roughness changes the metal response");
    expect(cDielectricEnv > cDielectric + 30, "IBL environment brightens the ambient term");

    // --- deferred path: 5-attachment G-buffer + PBR lighting pass --------------------
    fuhgl::render::DeferredPipeline deferred;
    if (!deferred.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_pbr: deferred pipeline create failed\n");
        return 1;
    }
    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.f, 0.f, 0.f};
    scene.ambient = {0.06f, 0.06f, 0.06f};
    scene.items.push_back({&box, &metal, fuhgl::math::Mat4{}});
    scene.lights.push_back(sun);
    scene.env = &env;
    deferred.render(scene);
    fuhgl::gl::flush();
    unsigned char dpx[4] = {0, 0, 0, 0};
    deferred.output().bind();
    fuhgl::gl::readPixels(W / 2, H / 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, dpx);
    std::fprintf(stderr, "gl_smoke_pbr: deferred center=%d\n", dpx[0]);
    expect(dpx[0] > 40, "deferred PBR lighting pass renders the metal box (5th G-buffer attachment works)");

    // G-buffer PBR texture carries the material values (metallic=1 -> R=255 at the box center)
    fuhgl::gl::activeTexture(fuhgl::gl::GL_TEXTURE0 + fuhgl::render::kTextureUnitGBufferPbr);
    fuhgl::gl::functions().glBindTexture(fuhgl::gl::GL_TEXTURE_2D, deferred.texPbr().id());
    std::vector<unsigned char> pbrAll(W * H * 4, 0);
    fuhgl::gl::getTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, pbrAll.data());
    const unsigned char centerMetallic = pbrAll[(H / 2 * W + W / 2) * 4 + 0];
    std::fprintf(stderr, "gl_smoke_pbr: gbuffer center metallic=%d\n", centerMetallic);
    expect(centerMetallic > 240, "G-buffer PBR attachment stores metallic=1 at the box center");

    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_pbr: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_pbr: OK\n");
    return 0;
}
