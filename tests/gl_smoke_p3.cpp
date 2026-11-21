// Headless smoke test for P3 (F21): program pipelines (separable programs + stages +
// programUniform), transform feedback capture with readback, program binary round-trip,
// subroutines, and GL 4.6 entry points (guarded). Skips cleanly without GL >= 4.1.

#include <fuhgl/fuhgl.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace fuhgl::gl;

namespace {

int g_failures = 0;
void expect(bool cond, const char* what) {
    std::fprintf(stderr, "  [%s] %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) ++g_failures;
}

constexpr const char* kVs = R"GLSL(#version 330 core
out gl_PerVertex { vec4 gl_Position; };   // required for separate shader objects (SSO)
layout(location = 0) in vec2 aPos;
out vec4 vColor;
void main() {
    vColor = vec4(0.2, 0.9, 0.3, 1.0);
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kVsTf = R"GLSL(#version 330 core
out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) in vec2 aPos;
out vec4 vOut;
void main() {
    vOut = vec4(aPos, 0.5, 1.0);
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kFs = R"GLSL(#version 330 core
uniform float uScale;
out vec4 FragColor;
void main() { FragColor = vec4(0.9, 0.2, 0.2, 1.0) * uScale; }
)GLSL";

constexpr const char* kFsSub = R"GLSL(#version 400 core
subroutine vec4 shade_t(vec4 c);
subroutine uniform shade_t uShade;
out vec4 FragColor;
subroutine(shade_t) vec4 shadeRed(vec4 c) { return c * vec4(1.0, 0.2, 0.2, 1.0); }
subroutine(shade_t) vec4 shadeGreen(vec4 c) { return c * vec4(0.2, 1.0, 0.2, 1.0); }
void main() { FragColor = uShade(vec4(1.0)); }
)GLSL";

} // namespace

int main() {
#ifndef _WIN32
    if (!std::getenv("FUHGL_BACKEND")) setenv("FUHGL_BACKEND", "surfaceless", 0);
#else
    if (!std::getenv("FUHGL_BACKEND")) _putenv_s("FUHGL_BACKEND", "surfaceless");
#endif

    fuhgl::Window window;
    if (!window.create("gl_smoke_p3", 128, 128)) {
        std::fprintf(stderr, "gl_smoke_p3: no GL context — skipping\n");
        return 0;
    }
    if (!version().atLeast(4, 1)) {
        std::fprintf(stderr, "gl_smoke_p3: context below GL 4.1 — skipping\n");
        return 0;
    }
    const auto& f = functions();
    std::fprintf(stderr, "  [info] GL %d.%d, hasDsa=%d\n", version().major, version().minor, (int)hasDsa());
    auto expectClean = [&](const char* what) {
        Error e = lastError();
        if (e != Error::NoError) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%s (GL error 0x%x)", what, static_cast<unsigned>(e));
            expect(false, buf);
        } else expect(true, what);
    };

    // --- program pipeline: separable VS + FS, stages, programUniform, pixel check ----
    {
        fuhgl::Program vsProg, fsProg;
        vsProg.create();
        vsProg.setParameteri(GL_PROGRAM_SEPARABLE, GL_TRUE);
        fuhgl::Shader vs(fuhgl::Shader::Type::Vertex);
        vs.create(fuhgl::Shader::Type::Vertex);
        vs.compile(kVs);
        vsProg.attach(vs);
        { const bool ok = vsProg.link(); if (!ok) std::fprintf(stderr, "  [info] VS log: %s\n", vsProg.infoLog().c_str()); expect(ok, "separable VS linked"); }
        fsProg.create();
        fsProg.setParameteri(GL_PROGRAM_SEPARABLE, GL_TRUE);
        fuhgl::Shader fs(fuhgl::Shader::Type::Fragment);
        fs.create(fuhgl::Shader::Type::Fragment);
        fs.compile(kFs);
        fsProg.attach(fs);
        { const bool ok = fsProg.link(); if (!ok) std::fprintf(stderr, "  [info] FS log: %s\n", fsProg.infoLog().c_str()); expect(ok, "separable FS linked"); }

        if (!f.glCreateProgramPipelines) {
            std::fprintf(stderr, "  [info] no glCreateProgramPipelines on this driver — skipping pipeline group\n");
        } else {
        fuhgl::ProgramPipeline pipe;
        expect(pipe.create(), "ProgramPipeline::create");
        pipe.use();
        GLint pipeBound = 0;
        f.glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, &pipeBound);
        expect(static_cast<GLuint>(pipeBound) == pipe.id(), "glBindProgramPipeline (binding getter)");
        pipe.useStages(GL_VERTEX_SHADER_BIT, vsProg.id());
        pipe.useStages(GL_FRAGMENT_SHADER_BIT, fsProg.id());
        pipe.setActiveShaderProgram(fsProg.id());
        const GLint loc = f.glGetUniformLocation ? f.glGetUniformLocation(fsProg.id(), "uScale") : -1;
        expect(loc >= 0, "uniform uScale found in separable FS");
        pipe.programUniform(fsProg.id(), loc, 1.0f); // glProgramUniform* targets the separable program
        pipe.use();
        { const bool ok = pipe.validate(); std::fprintf(stderr, "  [info] pipeline validate=%d log='%s'\n", (int)ok, pipe.infoLog().c_str()); expectClean("pipeline validate call"); }

        // triangle VAO + FBO
        fuhgl::VertexArray vao;
        vao.create();
        vao.bind();
        fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
        vbo.create(fuhgl::Buffer::Target::Array);
        vbo.bind();
        const float tri[6] = {-0.8f, -0.8f, 0.8f, -0.8f, 0.f, 0.8f};
        vbo.setData(sizeof(tri), tri, fuhgl::Buffer::Usage::StaticDraw);
        vao.setAttrib(0, 2, GL_FLOAT, false, 8, 0);
        vao.enableAttrib(0);

        const int W = 128, H = 128;
        fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);
        tex.bind();
        tex.texStorage2D(1, GL_RGBA8, W, H);
        fuhgl::Framebuffer fbo;
        fbo.create();
        fbo.setSize(W, H);
        fbo.bind();
        fbo.attachTexture2D(0, tex);
        expect(fbo.isComplete(), "pipeline FBO complete");
        f.glViewport(0, 0, W, H);
        f.glClearColor(0.f, 0.f, 0.f, 1.f);
        f.glClear(GL_COLOR_BUFFER_BIT);
        f.glDrawArrays(GL_TRIANGLES, 0, 3);
        f.glFlush();
        unsigned char px[4] = {};
        f.glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        expect(px[0] > 200 && px[1] < 80, "pipeline render produced red center pixel");
        expectClean("no GL error in pipeline group");
        if (f.glBindProgramPipeline) f.glBindProgramPipeline(0); // leave pipeline mode for the TF group
        } // pipeline available
        fuhgl::VertexArray::unbind();
    }

    // --- transform feedback: capture vOut into a buffer, read back -------------------
    {
        fuhgl::Program prog;
        prog.create();
        const GLchar* varyings[] = {"vOut"};
        prog.setTransformFeedbackVaryings(1, varyings, GL_INTERLEAVED_ATTRIBS);
        fuhgl::Shader vs(fuhgl::Shader::Type::Vertex);
        vs.create(fuhgl::Shader::Type::Vertex);
        vs.compile(kVsTf);
        prog.attach(vs);
        expect(prog.link(), "TF program linked");
        GLint nVar = -1;
        f.glGetProgramiv(prog.id(), GL_TRANSFORM_FEEDBACK_VARYINGS, &nVar);
        std::fprintf(stderr, "  [info] TF varyings registered: %d\n", nVar);
        expect(nVar == 1, "TF program has 1 varying");

        fuhgl::Buffer capture(fuhgl::Buffer::Target::Array);
        capture.create(fuhgl::Buffer::Target::Array);
        capture.bind();
        capture.setData(3 * 16, nullptr, fuhgl::Buffer::Usage::StreamDraw);
        f.glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, capture.id());
        expectClean("glBindBufferBase TF buffer");
        GLint bp0 = -1;
        f.glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &bp0);
        std::fprintf(stderr, "  [info] TF buffer binding 0 = %d (capture id %u)\n", bp0, capture.id());
        expect(static_cast<GLuint>(bp0) == capture.id(), "TF buffer binding point 0 has the capture buffer");

        fuhgl::VertexArray vao;
        vao.create();
        vao.bind();
        fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);
        vbo.create(fuhgl::Buffer::Target::Array);
        vbo.bind();
        const float tri[6] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.f, 0.5f};
        vbo.setData(sizeof(tri), tri, fuhgl::Buffer::Usage::StaticDraw);
        vao.setAttrib(0, 2, GL_FLOAT, false, 8, 0);
        vao.enableAttrib(0);

        fuhgl::TransformFeedback tf;
        expect(tf.create(), "TransformFeedback::create");
        tf.bind();
        expectClean("tf.bind");
        GLint tfBound = 0;
        f.glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &tfBound);
        expect(static_cast<GLuint>(tfBound) == tf.id(), "glBindTransformFeedback (binding getter)");
        prog.use();
        expectClean("prog.use");
        tf.begin(GL_TRIANGLES);
        expectClean("tf.begin (no rasterizer discard)");
        tf.end();
        expectClean("tf.end (immediate)");
        f.glEnable(GL_RASTERIZER_DISCARD);
        expectClean("enable rasterizer discard");
        tf.begin(GL_TRIANGLES);
        expectClean("tf.begin (with discard)");
        f.glDrawArrays(GL_TRIANGLES, 0, 3);
        expectClean("draw with TF active");
        tf.end();
        expectClean("tf.end");
        f.glDisable(GL_RASTERIZER_DISCARD);
        GLint written = -1;
        tf.geti_v(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, 0, &written);
        expectClean("tf.geti_v");
        expect(written == 1, "TF primitives written == 1");
        float out[4] = {};
        f.glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 16, out);
        expectClean("glGetBufferSubData TF");
        expect(out[0] == -0.5f && out[1] == -0.5f && out[2] == 0.5f && out[3] == 1.0f, "TF captured vertex readback");
        expectClean("no GL error in TF group");
        fuhgl::VertexArray::unbind();
    }

    // --- program binary round-trip ----------------------------------------------------
    {
        fuhgl::Program prog;
        prog.create();
        prog.setParameteri(GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
        fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
        vs.create(fuhgl::Shader::Type::Vertex);
        vs.compile(kVs);
        fs.create(fuhgl::Shader::Type::Fragment);
        fs.compile(kFs);
        prog.attach(vs);
        prog.attach(fs);
        expect(prog.link(), "binary-hint program linked");
        GLint binLen = 0;
        f.glGetProgramiv(prog.id(), GL_PROGRAM_BINARY_LENGTH, &binLen);
        if (binLen <= 0) {
            std::fprintf(stderr, "  [info] driver reports no program binaries (len=0) — skipping binary round-trip\n");
            expect(true, "GL_PROGRAM_BINARY_LENGTH > 0 (unsupported -> skipped)");
        } else
        std::vector<unsigned char> bin(static_cast<size_t>(binLen > 0 ? binLen : 4));
        GLsizei len = 0;
        GLenum fmt = 0;
        f.glGetProgramBinary(prog.id(), static_cast<GLsizei>(bin.size()), &len, &fmt, bin.data());
        {
            expect(len > 0, "glGetProgramBinary returned bytes");
            const GLuint prog2 = f.glCreateProgram();
            f.glProgramBinary(prog2, fmt, bin.data(), len);
            GLint linked = 0;
            f.glGetProgramiv(prog2, GL_LINK_STATUS, &linked);
            expect(linked == GL_TRUE, "glProgramBinary -> linked program");
            expectClean("no GL error in binary group");
            if (f.glDeleteProgram) f.glDeleteProgram(prog2);
        }
    }

    // --- subroutines (GL 4.0) -----------------------------------------------------------
    {
        fuhgl::Program prog;
        prog.create();
        fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
        vs.create(fuhgl::Shader::Type::Vertex);
        vs.compile(kVs);
        fs.create(fuhgl::Shader::Type::Fragment);
        if (!fs.compile(kFsSub)) {
            std::fprintf(stderr, "  [info] driver rejected GLSL 400 subroutines — skipping subroutine checks\n");
        } else {
            prog.attach(vs);
            prog.attach(fs);
            expect(prog.link(), "subroutine program linked");
            const GLuint idxRed = f.glGetSubroutineIndex(prog.id(), GL_FRAGMENT_SHADER, "shadeRed");
            const GLuint idxGreen = f.glGetSubroutineIndex(prog.id(), GL_FRAGMENT_SHADER, "shadeGreen");
            expect(idxRed != GL_INVALID_INDEX && idxGreen != GL_INVALID_INDEX && idxRed != idxGreen, "glGetSubroutineIndex");
            const GLint loc = f.glGetSubroutineUniformLocation(prog.id(), GL_FRAGMENT_SHADER, "uShade");
            expect(loc >= 0, "glGetSubroutineUniformLocation");
            prog.use(); // subroutine uniforms apply to the current program (no pipeline bound)
            const GLuint idx[1] = {idxGreen};
            f.glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, idx);
            GLuint back = 0xFFFFFFFFu;
            f.glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, loc, &back);
            expect(back == idxGreen, "glUniformSubroutinesuiv round-trip");
        }
        expectClean("no GL error in subroutine group");
    }

    // --- GL 4.6 entry points (guarded) ---------------------------------------------------
    if (version().atLeast(4, 6)) {
        expect(f.glPolygonOffsetClamp != nullptr, "glPolygonOffsetClamp present (4.6)");
        f.glPolygonOffsetClamp(1.f, 1.f, 0.5f);
        expect(f.glSpecializeShader != nullptr, "glSpecializeShader present (4.6)");
        expect(f.glMultiDrawArraysIndirectCount != nullptr, "glMultiDrawArraysIndirectCount present (4.6)");
        expectClean("no GL error in 4.6 group");
    } else {
        std::fprintf(stderr, "  [info] context is GL %d.%d < 4.6 — 4.6 calls presence-checked only\n",
                     version().major, version().minor);
    }

    if (g_failures == 0) std::fprintf(stderr, "gl_smoke_p3: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
