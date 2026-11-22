// Unit tests for F36 DoF math: math::cocRadius (normalized circle of confusion).

#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/math.hpp>

using namespace fuhgl;

void runDofTests() {
    // in focus -> 0
    CHECK_NEAR(math::cocRadius(8.f, 8.f, 6.f), 0.f, 1e-6f);
    // beyond focus + range -> clamped to 1
    CHECK_NEAR(math::cocRadius(100.f, 8.f, 6.f), 1.f, 1e-6f);
    // in front of focus by more than range -> also 1 (symmetric)
    CHECK_NEAR(math::cocRadius(0.f, 8.f, 6.f), 1.f, 1e-6f);
    // halfway -> 0.5
    CHECK_NEAR(math::cocRadius(11.f, 8.f, 6.f), 0.5f, 1e-6f);
    CHECK_NEAR(math::cocRadius(5.f, 8.f, 6.f), 0.5f, 1e-6f);
    // degenerate range must not divide by zero
    CHECK(math::cocRadius(10.f, 8.f, 0.f) == 1.f);
    // monotonic in distance beyond focus
    CHECK(math::cocRadius(9.f, 8.f, 6.f) < math::cocRadius(12.f, 8.f, 6.f));
}
