// Headless smoke test for the F46 DeferredRenderer shadow-kit integration: CSM with the
// advanced sampling filters (poisson PCF / PCSS / VSM), texel-snapped + blended cascades,
// a point-light cube shadow and screen-space contact shadows. Verifies the shadow blob
// still darkens with each filter, that the point shadow and contact shadows run, and
// that no GL errors occur. Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>
#include <utility>
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
    if (!window.create("gl_smoke_deferred_kit", 192, 192)) {
        std::fprintf(stderr, "gl_smoke_deferred_kit: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_deferred_kit: context below GL 3.3 — SKIP\n");
        return 77;
    }

    using namespace fuhgl;
    using namespace fuhgl::render;

    const int W = 192, H = 192;
    DeferredRenderer renderer;
    if (!renderer.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_deferred_kit: renderer create failed\n");
        return 1;
    }
    renderer.setEffectEnabled(Effect::Ssao, false);   // keep the test focused on shadows
    renderer.setEffectEnabled(Effect::Bloom, false);
    renderer.setEffectEnabled(Effect::GodRays, false);
    renderer.enableCsmShadows(512, 4);
    renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 8.f);
    renderer.setShadowZRange(0.5f, 60.f);

    Mesh floorMesh, caster;
    shapes::box(floorMesh, {8.f, 0.25f, 8.f}, {0.6f, 0.6f, 0.62f});
    shapes::box(caster, {0.7f, 1.4f, 0.7f}, {0.9f, 0.35f, 0.25f});

    FirstPersonCamera cam;
    cam.position = {0.f, 4.f, 8.f};
    cam.yaw = 0.f;
    cam.pitch = -0.38f;

    SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.12f, 0.12f, 0.13f};
    scene.items.push_back({&floorMesh, nullptr, math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&caster, nullptr, math::translate({-0.5f, 1.4f, 0.2f})});
    scene.lights.push_back(Light::directional({0.55f, -1.f, 0.3f}, {1.f, 0.9f, 0.75f}, 1.5f));
    scene.lights.push_back(Light::point({3.f, 4.f, 3.f}, {0.5f, 0.6f, 1.f}, 12.f, 20.f)); // index 1

    auto renderImage = [&]() {
        renderer.render(scene);
        gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
        renderer.output().bind();
        gl::readPixels(0, 0, W, H, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };
    auto countDarker = [](const std::vector<unsigned char>& a, const std::vector<unsigned char>& b) {
        int darker = 0;
        for (size_t i = 0; i < a.size(); i += 4)
            if (static_cast<int>(b[i]) - static_cast<int>(a[i]) > 12) ++darker;
        return darker;
    };
    auto litCount = [](const std::vector<unsigned char>& px) {
        int lit = 0;
        for (size_t i = 0; i < px.size(); i += 4)
            if (px[i] > 100) ++lit;
        return lit;
    };

    // baseline: no shadows
    renderer.setEffectEnabled(Effect::CsmShadows, false);
    const auto noShadow = renderImage();
    expect(litCount(noShadow) > 500, "baseline: floor is lit");

    // each advanced filter must still darken the shadow blob
    const ShadowFilter filters[] = {ShadowFilter::Pcf3x3, ShadowFilter::PoissonPcf, ShadowFilter::Pcss,
                                    ShadowFilter::Vsm};
    const char* names[] = {"pcf3x3", "poisson", "pcss", "vsm"};
    for (int f = 0; f < 4; ++f) {
        renderer.setShadowFilter(filters[f]);
        renderer.setEffectEnabled(Effect::CsmShadows, true);
        const auto img = renderImage();
        const int darker = countDarker(img, noShadow);
        std::fprintf(stderr, "  filter %s: darker=%d lit=%d\n", names[f], darker, litCount(img));
        expect(darker > 100, names[f]);
        expect(gl::lastError() == gl::Error::NoError, "no GL errors after filter");
    }

    // texel-snapped + blended cascades run cleanly
    renderer.setShadowFilter(ShadowFilter::PoissonPcf);
    renderer.shadowSnapToTexels = true;
    renderer.shadowCascadeBlend = 0.25f;
    renderer.shadowLightSize = 1.5f;
    {
        const auto img = renderImage();
        expect(countDarker(img, noShadow) > 100, "snapped+blended cascades still shadow");
        expect(gl::lastError() == gl::Error::NoError, "no GL errors with snap+blend");
    }

    // point-light cube shadow (light index 1)
    renderer.enablePointShadow(1, 256);
    expect(renderer.pointShadowEnabled() && renderer.pointShadowMap() != nullptr, "point shadow enabled");
    {
        const auto img = renderImage();
        const int lit = litCount(img);
        std::fprintf(stderr, "  point shadow: lit=%d\n", lit);
        expect(lit > 500, "scene still lit with point shadow on");
        expect(gl::lastError() == gl::Error::NoError, "no GL errors with point shadow");
    }
    renderer.disablePointShadow();

    // contact shadows (sampleable depth path)
    renderer.contactShadowsEnabled = true;
    {
        renderer.render(scene);
        gl::flush();
        expect(gl::lastError() == gl::Error::NoError, "no GL errors with contact shadows");
        expect(renderer.pipeline().depthTexture() != nullptr, "sampleable G-buffer depth created");
    }
    renderer.contactShadowsEnabled = false;

    // effect toggle API covers the new effects
    expect(renderer.effectSupported(Effect::PointShadows) && renderer.effectSupported(Effect::ContactShadows),
           "new effects supported on GL >= 3.3");
    renderer.setEffectEnabled(Effect::ContactShadows, true);
    expect(renderer.effectEnabled(Effect::ContactShadows), "contact shadows toggle on");
    renderer.setEffectEnabled(Effect::ContactShadows, false);
    expect(!renderer.effectEnabled(Effect::ContactShadows), "contact shadows toggle off");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_deferred_kit: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_deferred_kit: OK\n");
    return 0;
}
