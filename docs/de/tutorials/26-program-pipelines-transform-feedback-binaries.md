# 26 — Programm-Pipelines, Transform Feedback, Shader-Binaries (P3)

> 🇬🇧 **English version:** [docs/en/tutorials/26-program-pipelines-transform-feedback-binaries.md](../../en/tutorials/26-program-pipelines-transform-feedback-binaries.md)

**Ziel:** die übrigen GL-4.x-Subsysteme aus dem Backlog: separable **Programm-Pipelines**
(4.1), **Transform Feedback** (4.0), **Shader-/Programm-Binaries** (4.1), **Subroutinen**
(4.0) und die **GL-4.6**-Einstiegspunkte — alle jetzt in der Loader-Tabelle.

## Programm-Pipelines (GL 4.1)

```cpp
fuhgl::Program vsProg, fsProg;                 // ein separables Programm pro Stage
vsProg.create();
vsProg.setParameteri(fuhgl::gl::GL_PROGRAM_SEPARABLE, fuhgl::gl::GL_TRUE);
// ... VS anhängen + linken ...
fsProg.create();
fsProg.setParameteri(fuhgl::gl::GL_PROGRAM_SEPARABLE, fuhgl::gl::GL_TRUE);
// ... FS anhängen + linken ...

fuhgl::ProgramPipeline pipe;
pipe.create();
pipe.useStages(fuhgl::gl::GL_VERTEX_SHADER_BIT, vsProg.id());
pipe.useStages(fuhgl::gl::GL_FRAGMENT_SHADER_BIT, fsProg.id());
pipe.setActiveShaderProgram(fsProg.id());
pipe.programUniform(fsProg.id(), uScaleLoc, 1.0f); // glProgramUniform* zielt auf das separable Programm
pipe.validate();
pipe.use();                                    // ersetzt glUseProgram für Pipeline-Rendering

// Stage-Kompilierung + Linken in einem Aufruf (glCreateShaderProgramv):
fuhgl::gl::GLuint prog = fuhgl::ProgramPipeline::createShaderProgram(
    fuhgl::gl::GL_FRAGMENT_SHADER, fsSource);
```

## Transform Feedback (GL 4.0)

```cpp
fuhgl::Program prog;
prog.create();
const fuhgl::gl::GLchar* varyings[] = {"vOut"};
prog.setTransformFeedbackVaryings(1, varyings, fuhgl::gl::GL_INTERLEAVED_ATTRIBS); // vor link()
// ... VS mit `out vec4 vOut;` anhängen + linken ...

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
tf.end();                                       // tf.pause()/resume() ebenfalls verfügbar
fuhgl::gl::functions().glDisable(fuhgl::gl::GL_RASTERIZER_DISCARD);
GLint written = 0;
tf.geti_v(fuhgl::gl::GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, 0, &written);
tf.draw(fuhgl::gl::GL_TRIANGLES);               // aufgezeichneten Stream ohne VS erneut abspielen
```

## Shader-/Programm-Binaries (GL 4.1)

```cpp
prog.setParameteri(fuhgl::gl::GL_PROGRAM_BINARY_RETRIEVABLE_HINT, fuhgl::gl::GL_TRUE);
// ... linken ...
std::vector<unsigned char> bin(1 << 16);
fuhgl::gl::GLsizei len = 0; fuhgl::gl::GLenum fmt = 0;
fuhgl::gl::functions().glGetProgramBinary(prog.id(), bin.size(), &len, &fmt, bin.data());
fuhgl::gl::GLuint prog2 = fuhgl::gl::functions().glCreateProgram();
fuhgl::gl::functions().glProgramBinary(prog2, fmt, bin.data(), len); // sofortiges Laden, kein Kompilieren
```

## GL 4.6

`glSpecializeShader` (Shader-Spezialisierungs-Konstanten), `glPolygonOffsetClamp`,
`glMultiDrawArraysIndirectCount` / `glMultiDrawElementsIndirectCount` sind in der Tabelle —
mit `gl::version().atLeast(4, 6)` guarded.

Tests: `tests/test_loader.cpp` Präsenzliste (alle P3-Einträge) und
`tests/gl_smoke_p3.cpp` — headless verifiziert ein pipeline-gerendertes Pixel, TF-Capture-
Readback, Binary-Roundtrip und Subroutinen-Index-Roundtrip (überspringt unterhalb von GL 4.1).
