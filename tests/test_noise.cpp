#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>

#include <cmath>

using namespace fuhgl::math;

void runNoiseTests() {
    const Noise n(42);

    // determinism: same seed, same values
    CHECK(n.perlin2(1.25f, -3.5f) == Noise(42).perlin2(1.25f, -3.5f));
    CHECK(n.perlin3(0.5f, 2.5f, -1.5f) == Noise(42).perlin3(0.5f, 2.5f, -1.5f));
    CHECK(n.fbm2(3.3f, 7.7f, 4) == Noise(42).fbm2(3.3f, 7.7f, 4));

    // different seeds give different fields
    CHECK(n.perlin2(1.25f, -3.5f) != Noise(43).perlin2(1.25f, -3.5f));

    // integer lattice points are exactly 0 (improved Perlin property)
    CHECK(n.perlin2(2.f, -5.f) == 0.f);
    CHECK(n.perlin3(-1.f, 3.f, 7.f) == 0.f);

    // range: hard bound 2 (raw improved Perlin), values actually vary, mean ~ 0
    double sum = 0.0, sumSq = 0.0;
    const int kSamples = 20000;
    float mn = 1e9f, mx = -1e9f;
    for (int i = 0; i < kSamples; ++i) {
        const float x = -50.f + 100.f * i / (kSamples - 1);
        const float y = -50.f + 100.f * ((i * 37) % kSamples) / (kSamples - 1);
        const float v = n.perlin2(x, y);
        CHECK(std::fabs(v) < 2.f);
        mn = v < mn ? v : mn;
        mx = v > mx ? v : mx;
        sum += v;
        sumSq += static_cast<double>(v) * v;
    }
    const double mean = sum / kSamples;
    const double stddev = std::sqrt(sumSq / kSamples - mean * mean);
    CHECK(std::fabs(mean) < 0.05);
    CHECK(stddev > 0.15);
    CHECK(mx > 0.5f && mn < -0.5f);

    // 3D: same sanity properties
    float mn3 = 1e9f, mx3 = -1e9f;
    for (int i = 0; i < 4000; ++i) {
        const float v = n.perlin3(i * 0.37f, i * 0.11f, i * 0.73f);
        CHECK(std::fabs(v) < 2.f);
        mn3 = v < mn3 ? v : mn3;
        mx3 = v > mx3 ? v : mx3;
    }
    CHECK(mx3 > 0.5f && mn3 < -0.5f);

    // fBm: one octave == perlin (normalized by total amplitude 1), stays in range
    CHECK_NEAR(n.fbm2(1.7f, -2.9f, 1), n.perlin2(1.7f, -2.9f), 1e-6f);
    for (int i = 0; i < 2000; ++i) {
        const float v = n.fbm2(i * 0.13f, -i * 0.29f, 5);
        CHECK(std::fabs(v) < 2.f);
    }
    for (int i = 0; i < 2000; ++i) {
        const float v = n.fbm3(i * 0.13f, i * 0.29f, i * 0.07f, 4);
        CHECK(std::fabs(v) < 2.f);
    }

    // one-shot free functions match the Noise object with the same seed
    CHECK(perlin2(0.8f, 1.9f, 7) == Noise(7).perlin2(0.8f, 1.9f));
    CHECK(perlin3(0.8f, 1.9f, -0.4f, 7) == Noise(7).perlin3(0.8f, 1.9f, -0.4f));
    CHECK(fbm2(0.8f, 1.9f, 3, 7) == Noise(7).fbm2(0.8f, 1.9f, 3));
    CHECK(fbm3(0.8f, 1.9f, -0.4f, 3, 7) == Noise(7).fbm3(0.8f, 1.9f, -0.4f, 3));

    // zero octaves -> 0
    CHECK(n.fbm2(1.f, 2.f, 0) == 0.f);
    CHECK(n.fbm3(1.f, 2.f, 3.f, 0) == 0.f);
}
