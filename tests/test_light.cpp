#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/render/light.hpp>

#include <cmath>
#include <vector>

using namespace fuhgl;
using namespace fuhgl::render;

void runLightTests() {
    // --- factories ---------------------------------------------------------------
    const Light pl = Light::point({1.f, 2.f, 3.f}, {1.f, 0.5f, 0.25f}, 20.f, 10.f);
    CHECK(pl.type == LightType::Point);
    CHECK(pl.position == math::Vec3(1.f, 2.f, 3.f));
    CHECK(pl.color == math::Vec3(1.f, 0.5f, 0.25f));
    CHECK_NEAR(pl.intensity, 20.f, 1e-6f);
    CHECK_NEAR(pl.range, 10.f, 1e-6f);

    const Light dl = Light::directional({0.f, -2.f, 0.f}, {1.f, 1.f, 1.f}, 2.f);
    CHECK(dl.type == LightType::Directional);
    CHECK_NEAR(math::length(dl.direction), 1.f, 1e-6f);
    CHECK_NEAR(dl.direction.y, -1.f, 1e-6f);
    CHECK_NEAR(dl.intensity, 2.f, 1e-6f);

    const Light sl = Light::spot({0.f, 5.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 0.8f}, 30.f, 0.4f, 0.7f);
    CHECK(sl.type == LightType::Spot);
    CHECK_NEAR(sl.coneInner, 0.4f, 1e-6f);
    CHECK_NEAR(sl.coneOuter, 0.7f, 1e-6f);
    CHECK_NEAR(sl.direction.y, -1.f, 1e-6f);

    // aggregate init stays a point light (backward compatible)
    const Light agg{{3.f, 3.f, 4.f}, {1.f, 0.9f, 0.7f}, 25.f};
    CHECK(agg.type == LightType::Point);
    CHECK(agg.position == math::Vec3(3.f, 3.f, 4.f));

    // --- attenuation ---------------------------------------------------------------
    Light p; // defaults: c=1, l=0.35, q=0.12, intensity=10
    CHECK_NEAR(p.attenuation(0.f), 10.f, 1e-4f);
    CHECK_NEAR(p.attenuation(2.f), 10.f / (1.f + 0.35f * 2.f + 0.12f * 4.f), 1e-4f);
    CHECK(p.attenuation(5.f) < p.attenuation(1.f)); // monotonic falloff
    CHECK_NEAR(dl.attenuation(0.f), 2.f, 1e-6f);
    CHECK_NEAR(dl.attenuation(100.f), 2.f, 1e-6f); // directional: distance-independent

    const Light pr = Light::point({0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, 10.f, 4.f);
    CHECK_NEAR(pr.attenuation(0.f), 10.f, 1e-4f);
    CHECK(pr.attenuation(4.f) < 1e-3f);          // range window kills it at the limit
    CHECK(pr.attenuation(2.f) > pr.attenuation(3.f));

    // --- spot cone factor ------------------------------------------------------------
    const Light s = Light::spot({0.f, 0.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, 10.f, 0.5f, 0.8f);
    CHECK_NEAR(s.coneFactor({0.f, 1.f, 0.f}), 1.f, 1e-5f);   // on the axis: inside inner cone
    CHECK_NEAR(s.coneFactor({1.f, 0.f, 0.f}), 0.f, 1e-5f);   // 90 deg: outside outer cone
    const float half = (0.5f + 0.8f) * 0.5f;                 // mid angle between inner and outer
    const math::Vec3 mid{std::sin(half), std::cos(half), 0.f};
    const float cf = s.coneFactor(mid);
    CHECK(cf > 0.01f && cf < 0.99f);                         // smooth in between
    CHECK_NEAR(p.coneFactor({1.f, 0.f, 0.f}), 1.f, 1e-6f);   // non-spot: always 1
    CHECK_NEAR(dl.coneFactor({1.f, 0.f, 0.f}), 1.f, 1e-6f);

    // --- packLightBlock ---------------------------------------------------------------
    std::vector<Light> lights{pl, dl, sl};
    LightBlock block{};
    const int n = packLightBlock(lights, block);
    CHECK(n == 3);
    CHECK(block.posIntensity[0] == math::Vec4({1.f, 2.f, 3.f}, 20.f));
    CHECK(block.color[1] == math::Vec4({1.f, 1.f, 1.f}, 1.f));
    CHECK_NEAR(block.dirType[0].w, 0.f, 1e-6f); // point
    CHECK_NEAR(block.dirType[1].w, 1.f, 1e-6f); // directional
    CHECK_NEAR(block.dirType[2].w, 2.f, 1e-6f); // spot
    CHECK_NEAR(block.dirType[2].y, -1.f, 1e-6f);
    CHECK_NEAR(block.params0[0].w, 10.f, 1e-6f);               // range
    CHECK_NEAR(block.params1[2].x, std::cos(0.4f), 1e-6f);      // cos(coneInner)
    CHECK_NEAR(block.params1[2].y, std::cos(0.7f), 1e-6f);      // cos(coneOuter)
    CHECK(packLightBlock({}, block) == 0);
    const std::vector<Light> many(kMaxLights + 10);
    CHECK(packLightBlock(many, block) == kMaxLights); // clamped
}
