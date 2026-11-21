# 02 — Shader, Programme und GLSL-`#include`

> 🇬🇧 **English version:** [docs/en/tutorials/02-shaders-programs-includes.md](../../en/tutorials/02-shaders-programs-includes.md)

**Ziel:** Shader sicher kompilieren/linken und GLSL ohne externes Tool in wiederverwendbare
Dateien aufteilen.

```cpp
fuhgl::Shader vs(fuhgl::Shader::Type::Vertex);   // auch Fragment, Geometry, (Compute via 09)
if (!vs.compile(src)) { std::fprintf(stderr, "%s", vs.infoLog().c_str()); }

fuhgl::Program prog;
prog.create();
prog.attach(vs); prog.attach(fs);
if (!prog.link()) { /* prog.infoLog() */ }
prog.use();

// Uniforms: gecachter Lookup + typisierte Setter
prog.setUniform("uViewProj", cam.viewProj(aspect));       // Mat4
prog.setUniform(prog.uniformLocation("uColor"), 1.f, 0.f, 0.f); // floats...
```

## GLSL-Includes (dependency-frei)

`Shader::compileWithIncludes(src, resolver)` expandiert `#include "path"`-Zeilen
(rekursiv, tiefenbegrenzt) über deinen Resolver — z. B. aus Dateien vom Datenträger:

```cpp
fuhgl::Shader vs(fuhgl::Shader::Type::Vertex);
vs.compileWithIncludes(kSrc, [](const std::string& path) {
    std::ifstream f("shaders/" + path);
    return std::string(std::istreambuf_iterator<char>(f), {});
});
```

`#version` bleibt die erste Zeile; fehlende Dateien expandieren zu leer. Unit-getestet in
`tests/test_shader_includes.cpp`.

Siehe auch: [03](03-buffers-and-vao.md) für Geometrie, [09](09-compute-shaders.md) für Compute.
