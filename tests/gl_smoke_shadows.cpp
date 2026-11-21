// Headless smoke test for F15 shadow mapping: renders a floor + two caster boxes with
// a directional light through ForwardPipeline, once with a ShadowMap and once without,
// and verifies that the shadowed pixels get darker (and only darker).
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
    if (!window.create("gl_smoke_shadows", 192, 192)) {
        std::fprintf(stderr, "gl_smoke_shadows: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_shadows: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 192, H = 192;
    fuhgl::render::ForwardPipeline pipeline;
    if (!pipeline.create()) {
        std::fprintf(stderr, "gl_smoke_shadows: pipeline create failed\n");
        return 1;
    }
    fuhgl::render::ShadowMap shadowMap;
    if (!shadowMap.create(256)) {
        std::fprintf(stderr, "gl_smoke_shadows: shadow map create failed\n");
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
        std::fprintf(stderr, "gl_smoke_shadows: target FBO incomplete\n");
        return 1;
    }
    pipeline.setTarget(&target);

    fuhgl::render::Mesh floorMesh, caster1, caster2;
    fuhgl::render::shapes::box(floorMesh, {6.f, 0.25f, 6.f}, {0.6f, 0.6f, 0.62f});
    fuhgl::render::shapes::box(caster1, {0.6f, 1.2f, 0.6f}, {0.9f, 0.35f, 0.25f});
    fuhgl::render::shapes::box(caster2, {0.8f, 0.8f, 0.8f}, {0.3f, 0.5f, 0.9f});

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 4.f, 8.f};
    cam.pitch = -0.38f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.10f, 0.10f, 0.11f};
    scene.items.push_back({&floorMesh, nullptr, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&caster1, nullptr, fuhgl::math::translate({-0.5f, 1.2f, 0.2f})});
    scene.items.push_back({&caster2, nullptr, fuhgl::math::translate({2.2f, 0.8f, -1.2f})});
    scene.lights.push_back(fuhgl::render::Light::directional({0.55f, -1.f, 0.3f}, {1.f, 0.9f, 0.75f}, 1.5f));

    const fuhgl::math::Mat4 lightVP = fuhgl::render::lightViewProj(scene.lights[0], {0.f, 0.5f, 0.f}, 7.f);
    shadowMap.render(scene, lightVP);

    auto renderImage = [&](bool withShadow) {
        scene.shadowMap = withShadow ? &shadowMap : nullptr;
        scene.lightVP = lightVP;
        scene.shadowLightIndex = 0;
        scene.shadowStrength = 1.f;
        pipeline.render(scene);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
        target.bind();
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };

    const std::vector<unsigned char> withShadow = renderImage(true);
    const std::vector<unsigned char> noShadow = renderImage(false);

    int darker = 0, brighter = 0, lit = 0;
    for (size_t i = 0; i < withShadow.size(); i += 4) {
        const int d = static_cast<int>(noShadow[i]) - static_cast<int>(withShadow[i]);
        if (d > 12) ++darker;
        if (d < -12) ++brighter;
        if (noShadow[i] > 100) ++lit;
    }
    std::fprintf(stderr, "gl_smoke_shadows: darker=%d brighter=%d litFloorPx=%d\n", darker, brighter, lit);
    expect(lit > 500, "floor is lit without shadow");
    expect(darker > 150, "shadow map darkens a visible blob of pixels");
    expect(brighter < darker / 4, "shadows only darken (no brightening artifacts)");
    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_shadows: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_shadows: OK\n");
    return 0;
}
