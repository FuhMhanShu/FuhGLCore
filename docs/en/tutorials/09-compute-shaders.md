# 09 — Compute shaders (GL 4.3)

**Goal:** run general-purpose GPU work (here: fill an SSBO) without a window.

```cpp
// GLSL: #version 430 core
// layout(local_size_x = 64) in;
// layout(std430, binding = 0) buffer Data { float values[]; };
// void main() { values[gl_GlobalInvocationID.x] = ...; }

fuhgl::Shader cs(fuhgl::Shader::Type::Compute);   // add Compute to Shader::Type
cs.compile(src);
fuhgl::Program prog; prog.create(); prog.attach(cs); prog.link(); prog.use();

fuhgl::Buffer ssbo(fuhgl::Buffer::Target::ShaderStorage);
ssbo.bind(); ssbo.setData(N * sizeof(float), nullptr, fuhgl::Buffer::Usage::DynamicDraw);
ssbo.bindBase(0);                                  // binding = 0

fuhgl::gl::dispatchCompute(N / 64, 1, 1);
fuhgl::gl::memoryBarrier(fuhgl::gl::GL_SHADER_STORAGE_BARRIER_BIT);
// read back: bind + glReadPixels-like via map, or copy to a PBO
```

Requires GL 4.3 — check `fuhgl::gl::version().atLeast(4, 3)` or
`fuhgl::gl::hasExtension("GL_ARB_compute_shader")` first.
