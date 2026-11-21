# 27 — Programm-Reflexion (aktive Uniforms, Attribute, Uniform-Blöcke)

> 🇬🇧 **English version:** [docs/en/tutorials/27-program-reflection.md](../../en/tutorials/27-program-reflection.md)

**Ziel:** das Interface eines gelinkten Programms zur Laufzeit inspizieren — Uniforms,
Vertex-Attribute, UBO-Blöcke — ohne hartkodierte Locations (F22, schließt den letzten
§7-Backlog-Punkt „Reflection“).

```cpp
fuhgl::Program prog;
// ... anhängen + linken ...

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

- `Program::UniformInfo` — Name, `type`, `size`, `blockIndex` (-1 = Default-Block),
  `offset`, `arrayStride` (via `glGetActiveUniform` + `glGetActiveUniformsiv`).
- `Program::AttributeInfo` — Name, Typ, Größe, Location (`glGetActiveAttrib` +
  `glGetAttribLocation`).
- `Program::UniformBlockInfo` — Name, Index, `dataSize`, `binding`, aktive Uniform-Indizes
  (`glGetActiveUniformBlockiv/Name`).
- Die rohe GL-4.3-Resource-API ist ebenfalls in der Loader-Tabelle:
  `glGetProgramInterfaceiv`, `glGetProgramResourceiv/Name/Index`, `glGetUniformIndices`,
  `glGetAttachedShaders` (Konstanten `GL_UNIFORM`, `GL_UNIFORM_BLOCK`, `GL_ACTIVE_RESOURCES`,
  `GL_RES_*`, …).

Tests: `tests/test_loader.cpp` Präsenzliste (f22[]) und `tests/gl_smoke_reflection.cpp` —
linkt headless ein Programm mit std140-UBO + einfachen Uniforms + Attributen und verifiziert
die reflektierten Namen/Typen/Locations/Blockgrößen (überspringt unterhalb von GL 3.3).
