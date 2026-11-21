# 27 — Program reflection (active uniforms, attributes, uniform blocks)

**Goal:** inspect a linked program's interface at runtime — uniforms, vertex
attributes, UBO blocks — without hardcoding locations (F22, closes the last §7 backlog
item "Reflection").

```cpp
fuhgl::Program prog;
// ... attach + link ...

for (const auto& u : prog.activeUniforms())
    printf("uniform %-20s type=0x%x size=%d block=%d offset=%d\n",
           u.name.c_str(), u.type, u.size, u.blockIndex, u.offset);
// uniform uColor           type=0x8b52 size=1 block=-1 offset=0
// uniform lights.uLightPos  type=0x8b52 size=1 block=0  offset=0

for (const auto& a : prog.activeAttributes())
    printf("attrib %-10s loc=%d type=0x%x size=%d\n", a.name.c_str(), a.location, a.type, a.size);

for (const auto& b : prog.uniformBlocks())
    printf("block %-10s index=%u dataSize=%d binding=%d active=%d\n",
           b.name.c_str(), b.index, b.dataSize, b.binding, b.activeUniforms);
```

- `Program::UniformInfo` — name, `type`, `size`, `blockIndex` (-1 = default block),
  `offset`, `arrayStride` (via `glGetActiveUniform` + `glGetActiveUniformsiv`).
- `Program::AttributeInfo` — name, type, size, location (`glGetActiveAttrib` +
  `glGetAttribLocation`).
- `Program::UniformBlockInfo` — name, index, `dataSize`, `binding`, active uniform
  indices (`glGetActiveUniformBlockiv/Name`).
- The raw GL 4.3 resource API is in the loader table too: `glGetProgramInterfaceiv`,
  `glGetProgramResourceiv/Name/Index`, `glGetUniformIndices`, `glGetAttachedShaders`
  (constants `GL_UNIFORM`, `GL_UNIFORM_BLOCK`, `GL_ACTIVE_RESOURCES`, `GL_RES_*`, …).

Tests: `tests/test_loader.cpp` presence list (f22[]) and `tests/gl_smoke_reflection.cpp` —
links a program with a std140 UBO + plain uniforms + attributes headless and verifies
the reflected names/types/locations/block sizes (skips below GL 3.3).
