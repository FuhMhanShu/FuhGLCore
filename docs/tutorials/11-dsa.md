# 11 — GL 4.5 direct state access (DSA)

**Goal:** on GL 4.5+ create and modify objects without binding them first.

```cpp
if (fuhgl::gl::hasDsa()) {
    const auto& dsa = fuhgl::gl::dsa();
    fuhgl::gl::GLuint buf = 0;
    dsa.glCreateBuffers(1, &buf);                       // no bind needed
    dsa.glNamedBufferData(buf, size, data, fuhgl::gl::GL_STATIC_DRAW);
    dsa.glNamedBufferSubData(buf, offset, size, data);

    fuhgl::gl::GLuint tex = 0;
    dsa.glCreateTextures(fuhgl::gl::GL_TEXTURE_2D, 1, &tex);
    dsa.glTextureStorage2D(tex, levels, fuhgl::gl::GL_RGBA8, w, h);
    dsa.glTextureSubImage2D(tex, 0, 0, 0, w, h, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, pixels);
    dsa.glBindTextureUnit(0, tex);                      // bind to unit without activeTexture

    fuhgl::gl::GLuint vao = 0;
    dsa.glCreateVertexArrays(1, &vao);
    dsa.glEnableVertexArrayAttrib(vao, 0);
    dsa.glVertexArrayAttribFormat(vao, 0, 3, fuhgl::gl::GL_FLOAT, fuhgl::gl::GL_FALSE, 0);
    dsa.glVertexArrayVertexBuffer(vao, 0, buf, 0, stride);

    fuhgl::gl::GLuint fbo = 0;
    dsa.glCreateFramebuffers(1, &fbo);
    dsa.glNamedFramebufferTexture(fbo, fuhgl::gl::GL_COLOR_ATTACHMENT0, tex, 0);
    bool ok = dsa.glCheckNamedFramebufferStatus(fbo, fuhgl::gl::GL_FRAMEBUFFER) == fuhgl::gl::GL_FRAMEBUFFER_COMPLETE;
}
```

All DSA pointers are resolved by name in `gl::init()`; `gl::hasDsa()` is true only on
GL ≥ 4.5 with the pointers present. Below 4.5 use the classic bind-based wrappers.

## Full DSA table (P2 / F18)

`gl::dsa()` now covers **72 GL 4.5 entry points** — every object family can be created,
modified and queried without binding:

| Family | Entry points |
| --- | --- |
| Buffers | `glCreateBuffers`, `glNamedBufferData/SubData`, `glNamedBufferStorage`, `glMapNamedBuffer(Range)`, `glUnmapNamedBuffer`, `glFlushMappedNamedBufferRange`, `glCopyNamedBufferSubData`, `glClearNamedBufferSubData`, `glInvalidateNamedBufferData/SubData`, `glGetNamedBufferParameteriv/i64v`, `glGetNamedBufferSubData` |
| Textures | `glCreateTextures`, `glTextureStorage2D/3D`, `glTextureStorage2D/3DMultisample`, `glTextureSubImage2D/3D`, `glCompressedTextureSubImage2D/3D`, `glCopyTextureSubImage2D/3D`, `glTextureParameteri/f/fv`, `glGenerateTextureMipmap`, `glGetTextureImage`, `glGetCompressedTextureImage`, `glGetTextureLevelParameteriv`, `glTextureBuffer(Range)`, `glBindTextureUnit` |
| Samplers | `glCreateSamplers`, `glSamplerParameteri/f/fv`, `glBindSampler(s)` |
| Vertex arrays | `glCreateVertexArrays`, `glEnableVertexArrayAttrib`, `glVertexArrayAttribFormat/IFormat/LFormat`, `glVertexArrayAttribBinding`, `glVertexArrayVertexBuffer`, `glVertexArrayBindingDivisor`, `glVertexArrayElementBuffer`, `glGetVertexArrayiv/Indexediv/Indexed64iv` |
| Framebuffers | `glCreateFramebuffers`, `glNamedFramebufferTexture(Layer)`, `glNamedFramebufferDrawBuffer(s)`, `glNamedFramebufferReadBuffer`, `glClearNamedFramebufferiv/uiv/fv/fi`, `glCheckNamedFramebufferStatus`, `glBlitNamedFramebuffer` |
| Renderbuffers | `glCreateRenderbuffers`, `glNamedRenderbufferStorage(Multisample)`, `glGetNamedRenderbufferParameteriv` |
| Queries / TF | `glCreateQueries`, `glCreateTransformFeedbacks` |

```cpp
// immutable, mappable buffer in three calls, no binding anywhere:
fuhgl::gl::GLuint buf = 0;
dsa.glCreateBuffers(1, &buf);
dsa.glNamedBufferStorage(buf, bytes, nullptr,
                         fuhgl::gl::GL_DYNAMIC_STORAGE_BIT | fuhgl::gl::GL_MAP_WRITE_BIT);
void* p = dsa.glMapNamedBufferRange(buf, 0, bytes, fuhgl::gl::GL_MAP_WRITE_BIT);
// ... write ...
dsa.glFlushMappedNamedBufferRange(buf, 0, bytes);
dsa.glUnmapNamedBuffer(buf);

// typed FBO clear without binding the framebuffer:
dsa.glClearNamedFramebufferfv(fbo, fuhgl::gl::GL_COLOR, 0, red);
dsa.glClearNamedFramebufferfi(fbo, fuhgl::gl::GL_DEPTH_STENCIL, 0, 1.0f, 0);
```

`gl::dsaFunctionNames()` / `gl::dsaFunctionNameCount()` expose the table for
diagnostics and tests. `tests/gl_smoke_dsa.cpp` exercises every family with readback
verification and skips cleanly below GL 4.5.
