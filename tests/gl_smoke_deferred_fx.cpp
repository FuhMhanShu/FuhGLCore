// Headless smoke test for F17 DeferredRenderer: G-buffer + SSAO + HDR lighting +
// bloom + god rays + ACES composite, with shadows and a parallax/bump floor.
// Verifies per effect that toggling it changes the expected pixels. Skips without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

int countDarker(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b, int thresh) {
    int n = 0;
    for (size_t i = 0; i < a.size(); i += 4)
        if (static_cast<int>(b[i]) - static_cast<int>(a[i]) > thresh) ++n;
    return n;
}
int countBrighter(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b, int thresh) {
    int n = 0;
    for (size_t i = 0; i < a.size(); i += 4)
        if (static_cast<int>(a[i]) - static_cast<int>(b[i]) > thresh) ++n;
    return n;
}
int countDiff(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b, int thresh) {
    int n = 0;
    for (size_t i = 0; i < a.size(); i += 4)
        if (std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])) > thresh) ++n;
    return n;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_deferred_fx", 192, 192)) {
        std::fprintf(stderr, "gl_smoke_deferred_fx: no GL context — skipping\n");
        return 0;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_deferred_fx: context below GL 3.3 — skipping\n");
        return 0;
    }

    const int W = 192, H = 192;
    fuhgl::render::DeferredRenderer renderer;
    renderer.ssaoSamples = 16;
    if (!renderer.create(W, H)) {
        std::fprintf(stderr, "gl_smoke_deferred_fx: renderer create failed\n");
        return 1;
    }
    renderer.enableShadows(256);
    renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 7.f);
    renderer.ssaoRadius = 0.8f;
    renderer.ssaoStrength = 1.5f;

    // parallax/bump floor: procedural height + derived normal map + bricks albedo
    const fuhgl::render::procedural::Image heightImg = fuhgl::render::procedural::noise(128, 128, 7u, 4);
    fuhgl::Texture heightTex = fuhgl::render::procedural::makeTexture(heightImg);
    fuhgl::Texture normalTex = fuhgl::render::procedural::makeNormalMap(heightImg, 2.5f);
    fuhgl::Texture bricksTex = fuhgl::render::procedural::makeTexture(
        fuhgl::render::procedural::bricks(256, 256, 6, 8, {0.55f, 0.3f, 0.22f}, {0.75f, 0.72f, 0.68f}));
    fuhgl::render::Material floorMat = fuhgl::render::materials::parallax(bricksTex, heightTex, 0.04f, 16);
    floorMat.normalMap = &normalTex;
    fuhgl::render::Material plainMat = fuhgl::render::materials::matte({0.6f, 0.55f, 0.5f});

    fuhgl::render::Mesh floorMesh, box, lamp;
    fuhgl::render::shapes::box(floorMesh, {6.f, 0.25f, 6.f}, {1.f, 1.f, 1.f});
    fuhgl::render::shapes::box(box, {0.8f, 1.2f, 0.8f}, {0.3f, 0.5f, 0.9f});
    fuhgl::render::shapes::box(lamp, {0.25f, 0.25f, 0.25f}, {1.f, 1.f, 1.f});
    fuhgl::render::Material lampMat = fuhgl::render::materials::emissive({1.f, 0.85f, 0.5f}, 8.f); // HDR bloom source

    fuhgl::FirstPersonCamera cam;
    cam.position = {0.f, 3.5f, 8.f};
    cam.pitch = -0.3f;

    fuhgl::render::SceneView scene;
    scene.camera = &cam;
    scene.width = W;
    scene.height = H;
    scene.aspect = 1.f;
    scene.clearColor = {0.05f, 0.06f, 0.09f};
    scene.ambient = {0.12f, 0.12f, 0.14f};
    scene.items.push_back({&floorMesh, &floorMat, fuhgl::math::translate({0.f, -0.25f, 0.f})});
    scene.items.push_back({&box, nullptr, fuhgl::math::translate({-1.6f, 1.2f, -0.5f})});
    scene.items.push_back({&lamp, &lampMat, fuhgl::math::translate({1.8f, 0.25f, 0.8f})});
    // sun low in front of the camera so the sun disk is inside the frame (god rays need it)
    scene.lights.push_back(fuhgl::render::Light::directional({-0.3f, -0.08f, 1.f}, {1.f, 0.9f, 0.75f}, 1.6f));
    scene.lights.push_back(fuhgl::render::Light::point({2.f, 2.5f, 2.f}, {0.5f, 0.65f, 1.f}, 10.f, 12.f));

    auto renderImage = [&](const fuhgl::render::Material* floorOverride) {
        fuhgl::render::SceneView s = scene;
        if (floorOverride) s.items[0].material = floorOverride;
        renderer.render(s);
        fuhgl::gl::flush();
        std::vector<unsigned char> px(static_cast<size_t>(W) * H * 4, 0);
        renderer.output().bind();
        fuhgl::gl::readPixels(0, 0, W, H, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px.data());
        return px;
    };

    const std::vector<unsigned char> base = renderImage(nullptr);
    int lit = 0;
    for (size_t i = 0; i < base.size(); i += 4)
        if (base[i] > 100) ++lit;
    const size_t corner = (static_cast<size_t>(H - 3) * W + 2) * 4; // top-left = sky
    std::fprintf(stderr, "gl_smoke_deferred_fx: litPx=%d corner=(%d,%d,%d)\n", lit, base[corner], base[corner + 1], base[corner + 2]);
    expect(lit > 300, "scene is lit (box/floor pixels present)");
    expect(base[corner + 2] > base[corner] + 4, "top corner shows the blue-ish sky");

    renderer.ssaoEnabled = false;
    const std::vector<unsigned char> noAO = renderImage(nullptr);
    renderer.ssaoEnabled = true;
    const int aoDarker = countDarker(base, noAO, 8);
    std::fprintf(stderr, "gl_smoke_deferred_fx: ssaoDarker=%d\n", aoDarker);
    expect(aoDarker > 20, "SSAO darkens creases/contact areas");

    const float savedBloom = renderer.bloomStrength;
    renderer.bloomStrength = 0.f;
    const std::vector<unsigned char> noBloom = renderImage(nullptr);
    renderer.bloomStrength = savedBloom;
    expect(countBrighter(base, noBloom, 10) > 20, "bloom brightens around the HDR lamp");

    renderer.godRaysEnabled = false;
    const std::vector<unsigned char> noRays = renderImage(nullptr);
    renderer.godRaysEnabled = true;
    // compare total red-channel energy in the top quarter of the frame (toward the sun)
    long sumOn = 0, sumOff = 0;
    int anyDiff = 0;
    for (int y = H - H / 4; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const size_t o = (static_cast<size_t>(y) * W + x) * 4;
            sumOn += base[o];
            sumOff += noRays[o];
            if (base[o] != noRays[o]) ++anyDiff;
        }
    const fuhgl::math::Vec3 sunDir = -fuhgl::math::normalize(fuhgl::math::Vec3{-0.3f, -0.08f, 1.f});
    const fuhgl::math::Vec2 suv = fuhgl::render::sunScreenUV(cam, sunDir, W, H);
    const int sx = std::min(W - 1, std::max(0, static_cast<int>(suv.x * W)));
    const int sy = std::min(H - 1, std::max(0, static_cast<int>(suv.y * H)));
    const size_t so = (static_cast<size_t>(sy) * W + sx) * 4;
    std::fprintf(stderr, "gl_smoke_deferred_fx: rays top-quarter R sum on=%ld off=%ld anyDiff=%d sunUV=(%.2f,%.2f) sunPx=(%d,%d,%d)\n",
                 sumOn, sumOff, anyDiff, suv.x, suv.y, base[so], base[so + 1], base[so + 2]);
    // debug: read the HDR buffer around the sun pixel (RGBA16F -> read as float)
    {
        renderer.output().bind();
        std::vector<float> hdr(static_cast<size_t>(W) * H * 4, 0.f);
        fuhgl::gl::activeTexture(fuhgl::gl::GL_TEXTURE0);
        renderer.hdrTexture().bind();
        fuhgl::gl::functions().glGetTexImage(fuhgl::gl::GL_TEXTURE_2D, 0, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_FLOAT, hdr.data());
        int maxR = 0, argx = 0, argy = 0;
        for (int y = sy - 4; y <= sy + 4; ++y)
            for (int x = sx - 4; x <= sx + 4; ++x) {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                const int r = static_cast<int>(hdr[(static_cast<size_t>(y) * W + x) * 4] * 255.f);
                if (r > maxR) { maxR = r; argx = x; argy = y; }
            }
        const size_t hp = (static_cast<size_t>(sy) * W + sx) * 4;
        std::fprintf(stderr, "gl_smoke_deferred_fx: hdr sunPx=(%.2f,%.2f,%.2f) maxR5x5=%d at (%d,%d)\n",
                     hdr[hp], hdr[hp + 1], hdr[hp + 2], maxR, argx, argy);
        // read back the lighting program's sun uniforms
        fuhgl::Program& lp = renderer.pipeline().lightingMaterial().program();
        float v3[3] = {0, 0, 0}, v1 = 0;
        fuhgl::gl::functions().glGetUniformfv(lp.id(), lp.uniformLocation("uSunColor"), v3);
        fuhgl::gl::functions().glGetUniformfv(lp.id(), lp.uniformLocation("uSunDir"), v3);
        std::fprintf(stderr, "gl_smoke_deferred_fx: uSunDir loc=%d val=(%.3f,%.3f,%.3f)\n",
                     lp.uniformLocation("uSunDir"), v3[0], v3[1], v3[2]);
        fuhgl::gl::functions().glGetUniformfv(lp.id(), lp.uniformLocation("uSunColor"), v3);
        fuhgl::gl::functions().glGetUniformf(lp.id(), lp.uniformLocation("uSunCos"), &v1);
        std::fprintf(stderr, "gl_smoke_deferred_fx: uSunColor loc=%d val=(%.3f,%.3f,%.3f) uSunCos=%.6f\n",
                     lp.uniformLocation("uSunColor"), v3[0], v3[1], v3[2], v1);
    }
    expect(sumOn > sumOff + 150, "god rays add light toward the sun");

    const std::vector<unsigned char> noParallax = renderImage(&plainMat);
    expect(countDiff(base, noParallax, 10) > 50, "parallax/bump floor differs from plain floor");

    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_deferred_fx: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_deferred_fx: OK\n");
    return 0;
}
