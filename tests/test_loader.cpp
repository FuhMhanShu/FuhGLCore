#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/gl/loader.hpp>

#include <cstring>
#include <set>
#include <string>

using namespace fuhgl::gl;

void runLoaderTests() {
    // parseVersion
    CHECK((parseVersion("4.6.0 NVIDIA 535.54.03") == Version{4, 6}));
    CHECK((parseVersion("3.3.0 Mesa 23.0.4") == Version{3, 3}));
    CHECK((parseVersion("2.1.2") == Version{2, 1}));
    CHECK((parseVersion("garbage") == Version{0, 0}));
    CHECK((parseVersion(nullptr) == Version{0, 0}));
    CHECK((Version{4, 6}.atLeast(3, 3)));
    CHECK((Version{4, 6}.atLeast(4, 6)));
    CHECK(!(Version{3, 3}.atLeast(4, 0)));

    // splitExtensions
    const auto ext = splitExtensions("GL_ARB_texture GL_EXT_framebuffer_object  GL_KHR_debug ");
    CHECK(ext.size() == 3);
    CHECK(ext[0] == "GL_ARB_texture");
    CHECK(ext[2] == "GL_KHR_debug");
    CHECK(splitExtensions("").empty());
    CHECK(splitExtensions(nullptr).empty());

    // curated function table: non-empty, unique, all gl-prefixed
    CHECK(functionNameCount() > 50);
    std::set<std::string> names;
    for (std::size_t i = 0; i < functionNameCount(); ++i) {
        const char* n = functionNames()[i];
        CHECK(n != nullptr);
        CHECK(std::strncmp(n, "gl", 2) == 0);
        names.insert(n);
    }
    CHECK(names.size() == functionNameCount()); // no duplicates

    // without a GL context, init() may fail (headless) but must not crash;
    // procAddress must return nullptr for nonsense names.
    CHECK(procAddress("glThisFunctionDoesNotExist12345") == nullptr);
}
