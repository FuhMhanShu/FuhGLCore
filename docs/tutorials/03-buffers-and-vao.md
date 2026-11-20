# 03 — Buffers and vertex arrays

**Goal:** upload vertex/index data and describe the layout once.

```cpp
fuhgl::Buffer vbo(fuhgl::Buffer::Target::Array);          // ElementArray for indices
vbo.bind();
vbo.setData(size, data, fuhgl::Buffer::Usage::StaticDraw); // DynamicDraw / StreamDraw
vbo.setSubData(offset, size, data);                        // partial update
void* p = vbo.map(fuhgl::Buffer::Access::WriteOnly);       // map/unmap
vbo.unmap();

fuhgl::VertexArray vao; vao.create(); vao.bind();
vao.setAttrib(0, 3, fuhgl::gl::GL_FLOAT, false, stride, offset); // enables the attrib
vao.setDivisor(1, 1);                                        // instanced attrib
// vao.setElementBuffer(ibo);                                // index buffer

fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, count);
fuhgl::gl::drawElements(fuhgl::gl::Primitive::Triangles, count, fuhgl::gl::GL_UNSIGNED_INT, nullptr);
fuhgl::gl::drawElementsInstanced(fuhgl::gl::Primitive::Triangles, count, fuhgl::gl::GL_UNSIGNED_INT, nullptr, instances);
```

Buffer targets: `Array` (VBO), `ElementArray` (IBO), `Uniform` (UBO — see
[08](08-uniform-buffers.md)), `ShaderStorage` (SSBO — see [09](09-compute-shaders.md)).
`bindBase(index)` binds UBO/SSBO to an indexed binding point.

RAII: the GL object is deleted in the destructor; wrappers are movable but not copyable.
