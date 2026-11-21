#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/render/environment.hpp>

#include <cmath>

using namespace fuhgl;
using namespace fuhgl::math;
using namespace fuhgl::render;

static CubemapData constantCubemap(int size, const Vec3& c) {
    CubemapData d;
    d.size = size;
    for (int f = 0; f < 6; ++f) {
        d.face[f].assign(size_t(size) * size * 4, 0.f);
        for (int i = 0; i < size * size; ++i) {
            d.face[f][i * 4 + 0] = c.x;
            d.face[f][i * 4 + 1] = c.y;
            d.face[f][i * 4 + 2] = c.z;
            d.face[f][i * 4 + 3] = 1.f;
        }
    }
    return d;
}

void runEnvironmentTests() {
    // radicalInverse / hammersley
    CHECK_NEAR(radicalInverse(0), 0.f, 1e-7f);
    CHECK_NEAR(radicalInverse(1), 0.5f, 1e-7f);
    CHECK_NEAR(radicalInverse(2), 0.25f, 1e-7f);
    CHECK_NEAR(radicalInverse(3), 0.75f, 1e-7f);
    const Vec2 h0 = hammersley(0, 4);
    CHECK_NEAR(h0.x, 0.f, 1e-7f);
    CHECK_NEAR(h0.y, 0.f, 1e-7f);
    const Vec2 h1 = hammersley(1, 4);
    CHECK_NEAR(h1.x, 0.25f, 1e-7f);
    CHECK_NEAR(h1.y, 0.5f, 1e-7f);

    // cubemapTexelDir: face centers hit the axes, all directions are unit length
    CHECK_NEAR(length(cubemapTexelDir(0, 0.5f, 0.5f) - Vec3{1, 0, 0}), 0.f, 1e-5f);
    CHECK_NEAR(length(cubemapTexelDir(1, 0.5f, 0.5f) - Vec3{-1, 0, 0}), 0.f, 1e-5f);
    CHECK_NEAR(length(cubemapTexelDir(2, 0.5f, 0.5f) - Vec3{0, 1, 0}), 0.f, 1e-5f);
    CHECK_NEAR(length(cubemapTexelDir(3, 0.5f, 0.5f) - Vec3{0, -1, 0}), 0.f, 1e-5f);
    CHECK_NEAR(length(cubemapTexelDir(4, 0.5f, 0.5f) - Vec3{0, 0, 1}), 0.f, 1e-5f);
    CHECK_NEAR(length(cubemapTexelDir(5, 0.5f, 0.5f) - Vec3{0, 0, -1}), 0.f, 1e-5f);
    for (int f = 0; f < 6; ++f)
        for (float u : {0.f, 0.25f, 0.5f, 0.9f, 1.f})
            for (float v : {0.f, 0.5f, 1.f})
                CHECK_NEAR(length(cubemapTexelDir(f, u, v)), 1.f, 1e-4f);

    // sampleCubemap on a constant cubemap returns the constant for any direction
    const CubemapData cc = constantCubemap(8, {0.5f, 0.25f, 0.75f});
    for (const Vec3 d : {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, -1}, normalize(Vec3{1, 2, 3}), Vec3{-0.3f, -0.7f, 0.6f}}) {
        const Vec3 s = sampleCubemap(cc, d);
        CHECK_NEAR(s.x, 0.5f, 1e-5f);
        CHECK_NEAR(s.y, 0.25f, 1e-5f);
        CHECK_NEAR(s.z, 0.75f, 1e-5f);
    }

    // importanceSampleGGX: roughness 0 -> the normal itself; samples are unit + deterministic
    const Vec3 n{0.f, 0.f, 1.f};
    const Vec3 g0 = importanceSampleGGX({0.3f, 0.7f}, 0.f, n);
    CHECK_NEAR(length(g0 - n), 0.f, 1e-5f);
    const Vec3 ga = importanceSampleGGX({0.3f, 0.7f}, 0.4f, n);
    const Vec3 gb = importanceSampleGGX({0.3f, 0.7f}, 0.4f, n);
    CHECK_NEAR(length(ga), 1.f, 1e-5f);
    CHECK_NEAR(length(ga - gb), 0.f, 1e-6f);
    CHECK(ga.z > 0.f); // stays in the hemisphere around n

    // geometrySchlickGGX
    CHECK_NEAR(geometrySchlickGGX(1.f, 0.2f), 1.f, 1e-6f);
    CHECK(geometrySchlickGGX(0.5f, 0.2f) > 0.f && geometrySchlickGGX(0.5f, 0.2f) <= 1.f + 1e-6f);

    // bakeEnvironment: constant cubemap -> irradiance/prefilter = constant, LUT sane, deterministic
    EnvironmentBake bake;
    CHECK(bakeEnvironment(cc, bake, 8, 16, 32));
    CHECK(bake.valid());
    // irradiance stores RGB only; constant input -> every channel near the constant color
    for (int f = 0; f < 6; ++f)
        for (std::size_t i = 0; i + 2 < bake.irradianceFace[f].size(); i += 3) {
            CHECK_NEAR(bake.irradianceFace[f][i + 0], 0.5f, 0.02f);
            CHECK_NEAR(bake.irradianceFace[f][i + 1], 0.25f, 0.02f);
            CHECK_NEAR(bake.irradianceFace[f][i + 2], 0.75f, 0.02f);
        }
    CHECK_NEAR(bake.prefilterFaceLevel[0][0], 0.5f, 0.05f); // level 0 = roughness 0
    for (std::size_t i = 0; i < bake.brdfLut.size(); ++i) {
        CHECK(bake.brdfLut[i] >= -1e-4f && bake.brdfLut[i] <= 1.f + 1e-4f);
    }
    // energy: scale + bias <= 1 per texel
    for (std::size_t p = 0; p + 1 < bake.brdfLut.size(); p += 2)
        CHECK(bake.brdfLut[p] + bake.brdfLut[p + 1] <= 1.f + 1e-3f);
    // determinism
    EnvironmentBake bake2;
    CHECK(bakeEnvironment(cc, bake2, 8, 16, 32));
    CHECK(bake.brdfLut == bake2.brdfLut);
    CHECK(bake.irradianceFace[0] == bake2.irradianceFace[0]);
    CHECK(bake.prefilterFaceLevel[3] == bake2.prefilterFaceLevel[3]);

    // invalid input
    EnvironmentBake bad;
    CHECK(!bakeEnvironment(CubemapData{}, bad, 8, 16, 32));
    CHECK(!bakeEnvironment(cc, bad, 0, 16, 32));
}
