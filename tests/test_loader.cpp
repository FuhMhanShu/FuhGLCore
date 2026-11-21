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

    // F9 entries (buffer/texture/image power-ups) must be in the curated table
    const char* f9[] = {
        "glMapBufferRange", "glFlushMappedBufferRange", "glCopyBufferSubData", "glGetBufferSubData",
        "glGetBufferParameteriv", "glGetBufferParameteri64v", "glInvalidateBufferData", "glInvalidateBufferSubData",
        "glVertexAttribIPointer", "glVertexAttribLPointer", "glBindVertexBuffer", "glBindVertexBuffers",
        "glVertexAttribBinding", "glVertexAttribFormat", "glVertexAttribIFormat", "glVertexAttribLFormat",
        "glVertexBindingDivisor", "glTexSubImage3D", "glCopyTexImage2D", "glCopyTexSubImage2D", "glCopyTexSubImage3D",
        "glGetTexImage", "glGetCompressedTexImage", "glGetTexLevelParameteriv", "glGetTexParameteriv", "glGetTexParameterfv",
        "glCompressedTexImage2D", "glCompressedTexImage3D", "glCompressedTexSubImage2D", "glCompressedTexSubImage3D",
        "glTexBuffer", "glTexBufferRange", "glTextureView", "glBindImageTexture", "glBindImageTextures",
        "glDispatchComputeIndirect", "glShaderStorageBlockBinding", "glGetShaderStorageBlockIndex",
        "glGetUniformiv", "glGetUniformfv", "glGetUniformuiv", "glUniform1ui", "glUniform4uiv",
    };
    for (const char* fn : f9) CHECK(names.count(fn) == 1);

    // procAddress contract: nullptr in -> nullptr out. (We must NOT assert nullptr
    // for unknown names: Mesa's glXGetProcAddress returns no-op stubs for those.)
    CHECK(procAddress(nullptr) == nullptr);
}
