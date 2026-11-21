#include "test_framework.hpp"
#include "tests.hpp"

#include <cstdio>

int main() {
    runMathTests();
    runCameraTests();
    runLoaderTests();
    runFrustumTests();
    runShaderIncludeTests();
    runInfoTests();
    runSolidTests();
    runLightTests();
    std::fprintf(stderr, "fuhgl_tests: %d checks, %d failures\n", fuhgltest::checks(), fuhgltest::failures());
    return fuhgltest::failures() == 0 ? 0 : 1;
}
