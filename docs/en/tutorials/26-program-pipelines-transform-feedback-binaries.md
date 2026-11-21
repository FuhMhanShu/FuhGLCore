# 26 — Program pipelines, transform feedback, shader binaries (P3)

**Goal:** the remaining GL 4.x subsystems from the backlog: separable **program
pipelines** (4.1), **transform feedback** (4.0), **shader/program binaries** (4.1),
**subroutines** (4.0) and the **GL 4.6** entry points — all now in the loader table.

## Program pipelines (GL 4.1)

```cpp
fuhgl::Program vsProg, fsProg;                 // one separable program per stage
vsProg.create();
vsProg.setParameteri(fuhgl::gl::GL_PROGRAM_SEPARABLE, fuhgl::gl::GL_TRUE);
// ... attach + link VS ...
fsProg.create();
fsProg.setParameteri(fuhgl::gl::GL_PROGRAM_SEPARABLE, fuhgl::gl::GL_TRUE);
// ... attach + link FS ...

fuhgl::ProgramPipeline pipe;
pipe.create();
pipe.useStages(fuhgl::gl::GL_VERTEX_SHADER_BIT, vsProg.id());
pipe.useStages(fuhgl::gl::GL_FRAGMENT_SHADER_BIT, fsProg.id());
pipe.setActiveShaderProgram(fsProg.id());
pipe.programUniform(fsProg.id(), uScaleLoc, 1.0f); // glProgramUniform* targets the separable program
pipe.validate();
pipe.use();                                    // replaces glUseProgram for pipeline rendering

// one-call stage compile+link (glCreateShaderProgramv):
fuhgl::gl::GLuint prog = fuhgl::ProgramPipeline::createShaderProgram(
    fuhgl::gl::GL_FRAGMENT_SHADER, fsSource);
```

## Transform feedback (GL 4.0)

```cpp
fuhgl::Program prog;
prog.create();
const fuhgl::gl::GLchar* varyings[] = {"vOut"};
prog.setTransformFeedbackVaryings(1, varyings, fuhgl::gl::GL_INTERLEAVED_ATTRIBS); // before link()
// ... attach VS with `out vec4 vOut;` + link ...

fuhgl::Buffer capture(fuhgl::gl::GL_TRANSFORM_FEEDBACK_BUFFER);
capture.create(fuhgl::gl::GL_TRANSFORM_FEEDBACK_BUFFER);
capture.setData(bytes, nullptr, fuhgl::Buffer::Usage::StreamDraw);
fuhgl::gl::functions().glBindBufferBase(fuhgl::gl::GL_TRANSFORM_FEEDBACK_BUFFER, 0, capture.id());

fuhgl::TransformFeedback tf;
tf.create();
tf.bind();
fuhgl::gl::functions().glEnable(fuhgl::gl::GL_RASTERIZER_DISCARD);
prog.use();
tf.begin(fuhgl::gl::GL_TRIANGLES);
fuhgl::gl::functions().glDrawArrays(fuhgl::gl::GL_TRIANGLES, 0, 3);
tf.end();                                       // tf.pause()/resume() also available
fuhgl::gl::functions().glDisable(fuhgl::gl::GL_RASTERIZER_DISCARD);
GLint written = 0;
tf.geti_v(fuhgl::gl::GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, 0, &written);
tf.draw(fuhgl::gl::GL_TRIANGLES);               // replay captured stream without VS
```

## Shader / program binaries (GL 4.1)

```cpp
prog.setParameteri(fuhgl::gl::GL_PROGRAM_BINARY_RETRIEVABLE_HINT, fuhgl::gl::GL_TRUE);
// ... link ...
std::vector<unsigned char> bin(1 << 16);
fuhgl::gl::GLsizei len = 0; fuhgl::gl::GLenum fmt = 0;
fuhgl::gl::functions().glGetProgramBinary(prog.id(), bin.size(), &len, &fmt, bin.data());
fuhgl::gl::GLuint prog2 = fuhgl::gl::functions().glCreateProgram();
fuhgl::gl::functions().glProgramBinary(prog2, fmt, bin.data(), len); // instant load, no compile
```

## GL 4.6

`glSpecializeShader` (shader specialization constants), `glPolygonOffsetClamp`,
`glMultiDrawArraysIndirectCount` / `glMultiDrawElementsIndirectCount` are in the
table — guard with `gl::version().atLeast(4, 6)`.

Tests: `tests/test_loader.cpp` presence list (all P3 entries) and
`tests/gl_smoke_p3.cpp` — headless run verifies a pipeline-rendered pixel, TF capture
readback, binary round-trip and subroutine index round-trip (skips below GL 4.1).
