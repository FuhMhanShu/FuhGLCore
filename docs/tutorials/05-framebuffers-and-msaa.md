# 05 — Framebuffers, renderbuffers and MSAA resolve

**Goal:** render offscreen and resolve multisampled targets.

```cpp
fuhgl::Framebuffer fbo; fbo.create(); fbo.bind();
fbo.attachTexture2D(fuhgl::Framebuffer::Attachment::Color0, colorTex, 0);
fbo.attachRenderbuffer(fuhgl::Framebuffer::Attachment::Depth, depthRb);
if (!fbo.isComplete()) { /* fbo.status() has the reason */ }

fuhgl::gl::viewport(0, 0, w, h);
fuhgl::gl::clear(...);
/* draw scene */

// MSAA: create multisampled attachments, then resolve into a normal texture
fuhgl::Texture msaaTex(fuhgl::Texture::Target::Tex2DMultisample);
msaaTex.bind();
msaaTex.storageMultisample2D(4, fuhgl::gl::GL_RGBA8, w, h);
fuhgl::Renderbuffer msaaDepth; msaaDepth.create(); msaaDepth.bind();
msaaDepth.storageMultisample(4, fuhgl::gl::GL_DEPTH24_STENCIL8, w, h);
/* attach to fbo, render, then: */
msaaFbo.blitTo(resolveFbo, w, h, w, h, fuhgl::gl::GL_COLOR_BUFFER_BIT, false);

// read back (used by gl_smoke test):
fuhgl::gl::readPixels(0, 0, w, h, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, data);

fuhgl::Framebuffer::unbind();   // back to the default framebuffer
```
