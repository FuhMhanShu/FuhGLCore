# 11 — GL 4.5 Direct State Access (DSA)

> 🇬🇧 **English version:** [docs/en/tutorials/11-dsa.md](../../en/tutorials/11-dsa.md)

**Ziel:** unter GL 4.5+ Objekte erzeugen und verändern, ohne sie zuerst zu binden.

```cpp
if (fuhgl::gl::hasDsa()) {
    const auto& dsa = fuhgl::gl::dsa();
    fuhgl::gl::GLuint buf = 0;
    dsa.glCreateBuffers(1, &buf);                       // kein Binden nötig
    dsa.glNamedBufferData(buf, size, data, fuhgl::gl::GL_STATIC_DRAW);
    dsa.glNamedBufferSubData(buf, offset, size, data);

    fuhgl::gl::GLuint tex = 0;
    dsa.glCreateTextures(fuhgl::gl::GL_TEXTURE_2D, 1, &tex);
    dsa.glTextureStorage2D(tex, levels, fuhgl::gl::GL_RGBA8, w, h);
    dsa.glTextureSubImage2D(tex, 0, 0, 0, w, h, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, pixels);
    dsa.glBindTextureUnit(0, tex);                      // an Einheit binden ohne activeTexture

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

Alle DSA-Zeiger werden in `gl::init()` per Name aufgelöst; `gl::hasDsa()` ist nur bei
GL ≥ 4.5 mit vorhandenen Zeigern true. Darunter die klassischen bind-basierten Wrapper
verwenden.

## Die komplette DSA-Tabelle (P2 / F18)

`gl::dsa()` deckt jetzt **72 GL-4.5-Einstiegspunkte** ab — jede Objektfamilie kann ohne
Binden erzeugt, verändert und abgefragt werden:

| Familie | Einstiegspunkte |
| --- | --- |
| Puffer | `glCreateBuffers`, `glNamedBufferData/SubData`, `glNamedBufferStorage`, `glMapNamedBuffer(Range)`, `glUnmapNamedBuffer`, `glFlushMappedNamedBufferRange`, `glCopyNamedBufferSubData`, `glClearNamedBufferSubData`, `glInvalidateNamedBufferData/SubData`, `glGetNamedBufferParameteriv/i64v`, `glGetNamedBufferSubData` |
| Texturen | `glCreateTextures`, `glTextureStorage2D/3D`, `glTextureStorage2D/3DMultisample`, `glTextureSubImage2D/3D`, `glCompressedTextureSubImage2D/3D`, `glCopyTextureSubImage2D/3D`, `glTextureParameteri/f/fv`, `glGenerateTextureMipmap`, `glGetTextureImage`, `glGetCompressedTextureImage`, `glGetTextureLevelParameteriv`, `glTextureBuffer(Range)`, `glBindTextureUnit` |
| Sampler | `glCreateSamplers`, `glSamplerParameteri/f/fv`, `glBindSampler(s)` |
| Vertex-Arrays | `glCreateVertexArrays`, `glEnableVertexArrayAttrib`, `glVertexArrayAttribFormat/IFormat/LFormat`, `glVertexArrayAttribBinding`, `glVertexArrayVertexBuffer`, `glVertexArrayBindingDivisor`, `glVertexArrayElementBuffer`, `glGetVertexArrayiv/Indexediv/Indexed64iv` |
| Framebuffer | `glCreateFramebuffers`, `glNamedFramebufferTexture(Layer)`, `glNamedFramebufferDrawBuffer(s)`, `glNamedFramebufferReadBuffer`, `glClearNamedFramebufferiv/uiv/fv/fi`, `glCheckNamedFramebufferStatus`, `glBlitNamedFramebuffer` |
| Renderbuffer | `glCreateRenderbuffers`, `glNamedRenderbufferStorage(Multisample)`, `glGetNamedRenderbufferParameteriv` |
| Queries / TF | `glCreateQueries`, `glCreateTransformFeedbacks` |

```cpp
// unwandelbarer, mappbarer Puffer in drei Aufrufen, ganz ohne Binden:
fuhgl::gl::GLuint buf = 0;
dsa.glCreateBuffers(1, &buf);
dsa.glNamedBufferStorage(buf, bytes, nullptr,
                         fuhgl::gl::GL_DYNAMIC_STORAGE_BIT | fuhgl::gl::GL_MAP_WRITE_BIT);
void* p = dsa.glMapNamedBufferRange(buf, 0, bytes, fuhgl::gl::GL_MAP_WRITE_BIT);
// ... beschreiben ...
dsa.glFlushMappedNamedBufferRange(buf, 0, bytes);
dsa.glUnmapNamedBuffer(buf);

// typisiertes FBO-Clear ohne das Framebuffer zu binden:
dsa.glClearNamedFramebufferfv(fbo, fuhgl::gl::GL_COLOR, 0, red);
dsa.glClearNamedFramebufferfi(fbo, fuhgl::gl::GL_DEPTH_STENCIL, 0, 1.0f, 0);
```

`gl::dsaFunctionNames()` / `gl::dsaFunctionNameCount()` stellen die Tabelle für Diagnose
und Tests bereit. `tests/gl_smoke_dsa.cpp` übt jede Familie mit Readback-Verifikation und
überspringt sauber unterhalb von GL 4.5.
