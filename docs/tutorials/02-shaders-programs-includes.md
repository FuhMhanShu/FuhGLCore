# 02 — Shaders, programs and GLSL `#include`

**Goal:** compile/link shaders safely and split GLSL into reusable files without
any external tool.

```cpp
fuhgl::Shader vs(fuhgl::Shader::Type::Vertex);   // also Fragment, Geometry, (Compute via 09)
if (!vs.compile(src)) { std::fprintf(stderr, "%s", vs.infoLog().c_str()); }

fuhgl::Program prog;
prog.create();
prog.attach(vs); prog.attach(fs);
if (!prog.link()) { /* prog.infoLog() */ }
prog.use();

// uniforms: cached lookup + typed setters
prog.setUniform("uViewProj", cam.viewProj(aspect));       // Mat4
prog.setUniform(prog.uniformLocation("uColor"), 1.f, 0.f, 0.f); // floats...
```

## GLSL includes (dependency-free)

`Shader::compileWithIncludes(src, resolver)` expands `#include "path"` lines
(recursively, depth-limited) using your resolver — e.g. reading from disk:

```cpp
fuhgl::Shader vs(fuhgl::Shader::Type::Vertex);
vs.compileWithIncludes(kSrc, [](const std::string& path) {
    std::ifstream f("shaders/" + path);
    return std::string(std::istreambuf_iterator<char>(f), {});
});
```

`#version` stays the first line; missing files expand to empty. Unit-tested in
`tests/test_shader_includes.cpp`.

See also: [03](03-buffers-and-vao.md) for geometry, [09](09-compute-shaders.md) for compute.
