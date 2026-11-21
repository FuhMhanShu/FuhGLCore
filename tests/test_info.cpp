#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/gl/info.hpp>

#include <cstring>
#include <string>

using namespace fuhgl::gl;

void runInfoTests() {
    // DeviceInfo is a plain value type: defaults are empty/zero
    DeviceInfo d;
    CHECK(d.vendor.empty());
    CHECK(d.renderer.empty());
    CHECK((d.version == Version{0, 0}));
    CHECK(d.extensions.empty());
    CHECK(d.limits.maxTextureSize2D == 0);
    CHECK(d.limits.maxUniformBufferSize == 0);
    CHECK(d.limits.maxComputeWorkGroupSize[0] == 0);
    CHECK(!d.features.coreProfile);
    CHECK(!d.features.dsa);
    CHECK(!d.hasExtension("GL_KHR_debug"));
    CHECK(!d.hasExtension(nullptr));

    // hasExtension: exact match only
    d.extensions = {"GL_ARB_texture_storage", "GL_KHR_debug", "GL_EXT_texture_filter_anisotropic"};
    CHECK(d.hasExtension("GL_KHR_debug"));
    CHECK(d.hasExtension("GL_ARB_texture_storage"));
    CHECK(!d.hasExtension("GL_KHR"));        // no prefix matching
    CHECK(!d.hasExtension("gl_khr_debug"));  // case-sensitive
    CHECK(!d.hasExtension(""));

    // report(): contains the key sections and values
    d.vendor = "Mesa";
    d.renderer = "llvmpipe (LLVM 17)";
    d.versionString = "4.5 (Core Profile) Mesa 23.0.4";
    d.glslVersion = "4.50";
    d.version = Version{4, 5};
    d.features.coreProfile = true;
    d.features.dsa = true;
    d.features.compute = true;
    d.limits.maxTextureSize2D = 16384;
    d.limits.maxDrawBuffers = 8;
    d.limits.maxUniformBufferSize = 65536;
    const std::string r = d.report();
    CHECK(r.find("OpenGL 4.5 (core profile)") != std::string::npos);
    CHECK(r.find("Vendor:     Mesa") != std::string::npos);
    CHECK(r.find("Renderer:   llvmpipe (LLVM 17)") != std::string::npos);
    CHECK(r.find("GLSL:       4.50") != std::string::npos);
    CHECK(r.find("Extensions: 3") != std::string::npos);
    CHECK(r.find("DSA 4.5") != std::string::npos);
    CHECK(r.find("compute") != std::string::npos);
    CHECK(r.find("max 2D texture size") != std::string::npos);
    CHECK(r.find("16384") != std::string::npos);
    CHECK(r.find("65536") != std::string::npos);

    // without GL context: queryDeviceInfo() returns the empty default
    if (!isInitialized()) {
        const DeviceInfo empty = queryDeviceInfo();
        CHECK(empty.vendor.empty());
        CHECK(empty.extensions.empty());
        CHECK((empty.version == Version{0, 0}));
    }
}
