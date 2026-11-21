// Headless smoke test for F31 RenderGraph: a two-pass graph (fill "a" with red, then
// scale "a" into "b") is compiled, executed and read back; a cyclic graph must fail
// compile(). Skips cleanly without GL >= 3.3.

#include <fuhgl/fuhgl.hpp>
#include <fuhgl/render.hpp>

#include <cstdio>
#include <cstdlib>

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_graph", 64, 64)) {
        std::fprintf(stderr, "gl_smoke_graph: no GL context — SKIP\n");
        return 77;
    }
    if (!fuhgl::gl::version().atLeast(3, 3)) {
        std::fprintf(stderr, "gl_smoke_graph: context below GL 3.3 — SKIP\n");
        return 77;
    }

    const int W = 64, H = 64;
    fuhgl::render::RenderGraph g;
    g.addResource("a", W, H, fuhgl::gl::GL_RGBA8);
    g.addResource("b", W, H, fuhgl::gl::GL_RGBA8);

    fuhgl::render::Material scale;
    if (!scale.create()) return 1;
    {
        fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
        if (!vs.compile(R"GLSL(#version 330 core
out vec2 vUV;
void main() {
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;
    vUV = pos * 0.5 + 0.5;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)GLSL")) return 1;
        if (!fs.compile(R"GLSL(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main() { FragColor = vec4(texture(uTex, vUV).rgb * 0.5 + vec3(0.0, 0.0, 0.5), 1.0); }
)GLSL")) return 1;
        scale.program().attach(vs);
        scale.program().attach(fs);
        if (!scale.program().link()) return 1;
    }
    fuhgl::VertexArray vao;
    vao.create();
    vao.bind();

    g.addPass("fill", {}, {"a"}, [](fuhgl::render::RenderGraph::PassContext&) {
        fuhgl::gl::disable(fuhgl::gl::Cap::DepthTest);
        fuhgl::gl::clearColor(1.f, 0.f, 0.f, 1.f);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color);
    });
    g.addPass("scale", {"a"}, {"b"}, [&](fuhgl::render::RenderGraph::PassContext& ctx) {
        ctx.bindInput(0, "a");
        fuhgl::gl::disable(fuhgl::gl::Cap::DepthTest);
        scale.bind();
        scale.program().setUniform(scale.program().uniformLocation("uTex"), 0);
        vao.bind();
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
    });

    expect(g.compile(), "graph compiles");
    expect(g.order().size() == 2, "two passes in the execution order");
    g.execute();
    fuhgl::gl::flush();

    // read back "b": expect R ~ 0.5, B ~ 0.5, G ~ 0
    fuhgl::Framebuffer readFbo;
    readFbo.create();
    readFbo.setSize(W, H);
    readFbo.bind();
    readFbo.attachTexture2D(0, *g.texture("b"));
    expect(readFbo.isComplete(), "readback FBO complete");
    unsigned char px[4] = {0, 0, 0, 0};
    fuhgl::gl::readPixels(W / 2, H / 2, 1, 1, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, px);
    std::fprintf(stderr, "gl_smoke_graph: b center = (%d, %d, %d, %d)\n", px[0], px[1], px[2], px[3]);
    expect(px[0] > 110 && px[0] < 145, "red channel scaled by 0.5 (~127)");
    expect(px[2] > 110 && px[2] < 145, "blue channel added (~127)");
    expect(px[1] < 20, "green stays 0");

    // cyclic graph must fail compile
    fuhgl::render::RenderGraph bad;
    bad.addResource("x", W, H, fuhgl::gl::GL_RGBA8);
    bad.addResource("y", W, H, fuhgl::gl::GL_RGBA8);
    bad.addPass("p1", {"y"}, {"x"}, [](fuhgl::render::RenderGraph::PassContext&) {});
    bad.addPass("p2", {"x"}, {"y"}, [](fuhgl::render::RenderGraph::PassContext&) {});
    expect(!bad.compile(), "cyclic graph rejected by compile()");

    expect(fuhgl::gl::lastError() == fuhgl::gl::Error::NoError, "no GL errors");

    if (g_failures) {
        std::fprintf(stderr, "gl_smoke_graph: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "gl_smoke_graph: OK\n");
    return 0;
}
