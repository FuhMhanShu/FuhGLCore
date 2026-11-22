// Unit tests for F37 volumetric fog math: math::henyeyGreenstein phase function.

#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>

#include <initializer_list>

using namespace fuhgl;

void runVolFogTests() {
    // g = 0 -> isotropic: p = 1 / (4*pi) everywhere
    CHECK_NEAR(math::henyeyGreenstein(1.f, 0.f), 1.f / (4.f * math::kPi), 1e-6f);
    CHECK_NEAR(math::henyeyGreenstein(-1.f, 0.f), 1.f / (4.f * math::kPi), 1e-6f);
    CHECK_NEAR(math::henyeyGreenstein(0.f, 0.f), 1.f / (4.f * math::kPi), 1e-6f);
    // g > 0 -> forward scattering: p(1) > p(0) > p(-1)
    CHECK(math::henyeyGreenstein(1.f, 0.6f) > math::henyeyGreenstein(0.f, 0.6f));
    CHECK(math::henyeyGreenstein(0.f, 0.6f) > math::henyeyGreenstein(-1.f, 0.6f));
    // g < 0 -> backscattering: p(-1) > p(1)
    CHECK(math::henyeyGreenstein(-1.f, -0.6f) > math::henyeyGreenstein(1.f, -0.6f));
    // monotonic in cosTheta for g > 0
    CHECK(math::henyeyGreenstein(-0.5f, 0.7f) < math::henyeyGreenstein(0.5f, 0.7f));
    // never negative, finite for all g in (-1, 1)
    for (float g = -0.95f; g <= 0.95f; g += 0.19f)
        for (float c = -1.f; c <= 1.f; c += 0.25f) {
            const float p = math::henyeyGreenstein(c, g);
            CHECK(p >= 0.f && p < 1e6f);
        }
    // normalization: integral over the sphere = 1  (int p(cos) * 2*pi dcos over [-1, 1])
    for (float g : {-0.8f, -0.3f, 0.f, 0.5f, 0.9f}) {
        double sum = 0.0;
        const int n = 4000;
        const double dc = 2.0 / n;
        for (int i = 0; i < n; ++i) {
            const double c = -1.0 + (i + 0.5) * dc;
            sum += math::henyeyGreenstein(static_cast<float>(c), g) * 2.0 * math::kPi * dc;
        }
        CHECK_NEAR(sum, 1.0, 1e-3);
    }
    // stronger |g| -> more peaked forward lobe
    CHECK(math::henyeyGreenstein(1.f, 0.8f) > math::henyeyGreenstein(1.f, 0.2f));
}
