# 05 — Framebuffer, Renderbuffer und MSAA-Resolve

> 🇬🇧 **English version:** [docs/en/tutorials/05-framebuffers-and-msaa.md](../../en/tutorials/05-framebuffers-and-msaa.md)

**Ziel:** offscreen rendern und multisamplete Targets auflösen.

```cpp
fuhgl::Framebuffer fbo; fbo.create(); fbo.bind();
fbo.attachTexture2D(fuhgl::Framebuffer::Attachment::Color0, colorTex, 0);
fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depthRb);
if (!fbo.isComplete()) { /* fbo.status() enthält den Grund */ }

fuhgl::gl::viewport(0, 0, w, h);
fuhgl::gl::clear(...);
/* Szene zeichnen */

// MSAA: multisamplete Attachments erzeugen, dann in eine normale Textur auflösen
fuhgl::Texture msaaTex(fuhgl::Texture::Target::Tex2DMultisample);
msaaTex.bind();
msaaTex.storageMultisample2D(4, fuhgl::gl::GL_RGBA8, w, h);
fuhgl::Renderbuffer msaaDepth; msaaDepth.create(); msaaDepth.bind();
msaaDepth.storageMultisample(4, fuhgl::gl::GL_DEPTH24_STENCIL8, w, h);
/* an FBO anhängen, rendern, dann: */
msaaFbo.blitTo(resolveFbo, w, h, w, h, fuhgl::gl::GL_COLOR_BUFFER_BIT, false);

// Readback (wird vom gl_smoke-Test benutzt):
fuhgl::gl::readPixels(0, 0, w, h, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, data);

fuhgl::Framebuffer::unbind();   // zurück zum Default-Framebuffer
```
