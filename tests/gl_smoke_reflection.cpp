// Headless smoke test for F22 reflection: links a program with a UBO block, plain
// uniforms and vertex attributes, then verifies Program::activeUniforms(),
// activeAttributes() and uniformBlocks() against the known shader interface.
// Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace fuhgl::gl;

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

constexpr const char* kVs = R"GLSL(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(std140) uniform Lights {   // binding set via API (glUniformBlockBinding), GLSL 330-safe
    vec4 uLightPos;
    vec4 uLightColor;
} lights;
void main() {
    gl_Position = lights.uLightPos * vec4(aPos, 1.0) + vec4(aUV, 0.0, 0.0);
}
)GLSL";

constexpr const char* kFs = R"GLSL(#version 330 core
uniform vec4 uColor;
uniform float uScale;
out vec4 FragColor;
void main() { FragColor = uColor * uScale; }
)GLSL";

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_reflection", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_reflection: no GL context — skipping\n");
        return 0;
    }
    if (!version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_reflection: context below GL 3.3 — skipping\n");
        return 0;
    }

    fuhgl::Program prog;
    prog.create();
    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    vs.create(fuhgl::Shader::Type::Vertex);
    fs.create(fuhgl::Shader::Type::Fragment);
    expect(vs.compile(kVs), "VS compiles");
    expect(fs.compile(kFs), "FS compiles");
    prog.attach(vs);
    prog.attach(fs);
    expect(prog.link(), "program linked");

    // --- active attributes ---
    const auto attribs = prog.activeAttributes();
    bool foundPos = false, foundUV = false;
    for (const auto& a : attribs) {
        std::fprintf(stderr, "  [info] attrib '%s' type=0x%x size=%d loc=%d\n", a.name.c_str(),
                     (unsigned)a.type, a.size, a.location);
        if (a.name == "aPos" && a.location == 0 && a.type == GL_FLOAT_VEC3) foundPos = true;
        if (a.name == "aUV" && a.location == 1 && a.type == GL_FLOAT_VEC2) foundUV = true;
    }
    expect(foundPos, "active attribute aPos (loc 0, vec3)");
    expect(foundUV, "active attribute aUV (loc 1, vec2)");

    // --- active uniforms (default block + UBO members) ---
    const auto uniforms = prog.activeUniforms();
    bool foundColor = false, foundScale = false, foundLightPos = false;
    for (const auto& u : uniforms) {
        std::fprintf(stderr, "  [info] uniform '%s' type=0x%x size=%d block=%d offset=%d\n", u.name.c_str(),
                     (unsigned)u.type, u.size, u.blockIndex, u.offset);
        if (u.name == "uColor" && u.type == GL_FLOAT_VEC4 && u.size == 1 && u.blockIndex == -1) foundColor = true;
        if (u.name == "uScale" && u.type == GL_FLOAT && u.size == 1) foundScale = true;
        // drivers report the block-qualified name ("Lights.uLightPos"), some use the instance name
        if ((u.name == "lights.uLightPos" || u.name == "Lights.uLightPos") && u.blockIndex >= 0) foundLightPos = true;
    }
    expect(foundColor, "active uniform uColor (vec4, default block)");
    expect(foundScale, "active uniform uScale (float)");
    expect(foundLightPos, "active UBO member lights.uLightPos (blockIndex >= 0)");

    // --- uniform blocks ---
    const auto blocks = prog.uniformBlocks();
    expect(blocks.size() == 1, "exactly one uniform block");
    if (!blocks.empty()) {
        const auto& b = blocks[0];
        std::fprintf(stderr, "  [info] block '%s' index=%u dataSize=%d binding=%d active=%d\n", b.name.c_str(),
                     b.index, b.dataSize, b.binding, b.activeUniforms);
        expect(b.name == "Lights", "block name is 'Lights'");
        expect(b.dataSize == 32, "block data size == 32 (2 x vec4 std140)");
        expect(b.activeUniforms == 2 && b.uniformIndices.size() == 2, "block has 2 active uniforms");
    }

    // --- resource interface (GL 4.3, guarded) ---
    if (version().atLeast(4, 3)) {
        const auto& f = functions();
        if (f.glGetProgramInterfaceiv) {
            GLint nUniforms = 0;
            f.glGetProgramInterfaceiv(prog.id(), GL_UNIFORM, GL_ACTIVE_RESOURCES, &nUniforms);
            std::fprintf(stderr, "  [info] GL_UNIFORM interface active resources = %d\n", nUniforms);
            expect(nUniforms >= 3, "program resource interface: >= 3 active uniforms");
            expect(lastError() == Error::NoError, "no GL error in reflection group");
        }
    }

    if (g_failures == 0) std::fprintf(stderr, "gl_smoke_reflection: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
