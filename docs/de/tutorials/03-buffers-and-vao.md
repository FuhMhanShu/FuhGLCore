# 03 — Puffer und Vertex-Arrays

> 🇬🇧 **English version:** [docs/en/tutorials/03-buffers-and-vao.md](../../en/tutorials/03-buffers-and-vao.md)

**Ziel:** Vertex-/Indexdaten hochladen und das Layout einmal beschreiben.

```cpp
fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);          // ElementArray für Indizes
vbo.bind();
vbo.setData(size, data, fuhgl::Buffer::Usage::StaticDraw); // DynamicDraw / StreamDraw
vbo.setSubData(offset, size, data);                        // partielles Update
void* p = vbo.map(fuhgl::Buffer::Access::WriteOnly);       // map/unmap
vbo.unmap();

fuhgl::VertexArray vao; vao.create(); vao.bind();
vao.setAttrib(0, 3, fuhgl::gl::GL_FLOAT, false, stride, offset); // aktiviert das Attribut
vao.setDivisor(1, 1);                                        // instanziertes Attribut
// vao.setElementBuffer(ibo);                                // Index-Puffer

fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, count);
fuhgl::gl::drawElements(fuhgl::gl::Primitive::Triangles, count, fuhgl::gl::GL_UNSIGNED_INT, nullptr);
fuhgl::gl::drawElementsInstanced(fuhgl::gl::Primitive::Triangles, count, fuhgl::gl::GL_UNSIGNED_INT, nullptr, instances);
```

Puffer-Targets: `Array` (VBO), `ElementArray` (IBO), `Uniform` (UBO — siehe
[08](08-uniform-buffers.md)), `ShaderStorage` (SSBO — siehe [09](09-compute-shaders.md)).
`bindBase(index)` bindet UBO/SSBO an einen indexierten Bindepunkt.

RAII: das GL-Objekt wird im Destruktor gelöscht; die Wrapper sind verschiebbar, aber nicht
kopierbar.
