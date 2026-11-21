# 09 — Compute-Shader (GL 4.3)

> 🇬🇧 **English version:** [docs/en/tutorials/09-compute-shaders.md](../../en/tutorials/09-compute-shaders.md)

**Ziel:** allgemeine GPU-Arbeit ausführen (hier: einen SSBO füllen) — ohne Fenster.

```cpp
// GLSL: #version 430 core
// layout(local_size_x = 64) in;
// layout(std430, binding = 0) buffer Data { float values[]; };
// void main() { values[gl_GlobalInvocationID.x] = ...; }

fuhgl::Shader cs(fuhgl::Shader::Type::Compute);   // Compute zu Shader::Type hinzufügen
cs.compile(src);
fuhgl::Program prog; prog.create(); prog.attach(cs); prog.link(); prog.use();

fuhgl::Buffer ssbo(fuhgl::Buffer::Target::ShaderStorage);
ssbo.bind(); ssbo.setData(N * sizeof(float), nullptr, fuhgl::Buffer::Usage::DynamicDraw);
ssbo.bindBase(0);                                  // binding = 0

fuhgl::gl::dispatchCompute(N / 64, 1, 1);
fuhgl::gl::memoryBarrier(fuhgl::gl::GL_SHADER_STORAGE_BARRIER_BIT);
// Readback: binden + per map lesen (glReadPixels-artig) oder in einen PBO kopieren
```

Erfordert GL 4.3 — vorher `fuhgl::gl::version().atLeast(4, 3)` oder
`fuhgl::gl::hasExtension("GL_ARB_compute_shader")` prüfen.
