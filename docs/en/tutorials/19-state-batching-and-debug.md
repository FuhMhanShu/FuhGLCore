# 19 — GL state, draw batching, debug labels & image ops (F13)

**Goal:** the remaining everyday GL entry points, as thin `fuhgl::gl` free functions
(all in the curated loader table; no-ops when the driver lacks them): blending/stencil,
rasterizer state, viewport/scissor arrays, batched draws, object labels & debug groups,
queries/sync helpers, `glIsXxx`/`glGet*` getters, internal-format queries and GPU image
copies/clears.

## State (namespace `fuhgl::gl`, header `gl/state.hpp`)

```cpp
// blending / logic op
fuhgl::gl::enable(fuhgl::gl::Cap::Blend);
fuhgl::gl::blendEquation(fuhgl::gl::BlendEquation::FuncAdd);
fuhgl::gl::blendEquationSeparate(fuhgl::gl::BlendEquation::FuncAdd, fuhgl::gl::BlendEquation::Max);
fuhgl::gl::blendFuncSeparate(fuhgl::gl::BlendFactor::SrcAlpha, fuhgl::gl::BlendFactor::OneMinusSrcAlpha,
                             fuhgl::gl::BlendFactor::One, fuhgl::gl::BlendFactor::OneMinusSrcAlpha);
fuhgl::gl::blendColor(0, 0, 0, 1);
fuhgl::gl::logicOp(fuhgl::gl::LogicOp::Xor);              // needs Cap::Blend? no — logic op is its own path
fuhgl::gl::enablei(fuhgl::gl::Cap::Blend, 0);            // per-draw-buffer (GL 4.0)
fuhgl::gl::colorMaski(0, true, true, true, true);

// stencil
fuhgl::gl::enable(fuhgl::gl::Cap::StencilTest);
fuhgl::gl::stencilFunc(fuhgl::gl::CompareFunc::Always, 1, 0xFF);
fuhgl::gl::stencilOp(fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Replace);
fuhgl::gl::stencilMask(0xFF);
fuhgl::gl::stencilFuncSeparate(fuhgl::gl::Face::Front, ...); // face variants

// rasterizer / viewport / scissor
fuhgl::gl::polygonMode(fuhgl::gl::Face::FrontAndBack, fuhgl::gl::PolyMode::Line); // wireframe
fuhgl::gl::polygonOffset(1.f, 1.f);
fuhgl::gl::lineWidth(2.f); fuhgl::gl::pointSize(4.f);
fuhgl::gl::sampleCoverage(1.f, false); fuhgl::gl::sampleMaski(0, 0xFFFFFFFFu);
fuhgl::gl::minSampleShading(0.5f);
fuhgl::gl::viewportArrayv(0, 1, vp); fuhgl::gl::viewportIndexedf(1, 0, 0, 320, 240); // GL 4.1
fuhgl::gl::scissorArrayv(0, 1, sc);  fuhgl::gl::scissorIndexed(0, 0, 0, 64, 64);
fuhgl::gl::depthRangef(0.f, 0.5f); fuhgl::gl::depthRangeIndexed(0, 0.0, 1.0);
fuhgl::gl::provokingVertex(fuhgl::gl::ProvokingVertex::Last);
fuhgl::gl::primitiveRestartIndex(0xFFFFFFFFu);
fuhgl::gl::hint(fuhgl::gl::HintTarget::PolygonSmooth, fuhgl::gl::HintMode::Nicest);
fuhgl::gl::patchParameteri(fuhgl::gl::GL_PATCH_VERTICES, 3); // tessellation, GL_PATCHES primitive
```

`Primitive` gained `Patches` (`GL_PATCHES`).

## Framebuffer helpers (`Framebuffer` methods + free functions)

```cpp
fbo.attachTexture(Framebuffer::Attachment::Color0, arrayTex);        // whole array/cube (glFramebufferTexture)
fbo.attachTextureLayer(Framebuffer::Attachment::Color0, arrayTex, 0, 2); // one layer
int layer = fbo.attachmentParameteri(gl::GL_COLOR_ATTACHMENT0, gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER);
fbo.invalidate(1, &gl::GL_COLOR_ATTACHMENT0);                        // tiler GPUs: skip resolve
fbo.invalidateSub(1, &att, x, y, w, h);
gl::drawBuffer(gl::GL_COLOR_ATTACHMENT0);
gl::clearBufferfi(gl::GL_DEPTH_STENCIL, 0, 1.0f, 0);
gl::clearBufferuiv(gl::GL_COLOR, 0, u32);
```

## Draw batching

```cpp
gl::drawRangeElements(gl::Primitive::Triangles, 0, 99, 36, gl::GL_UNSIGNED_INT, nullptr);
gl::drawElementsBaseVertex(gl::Primitive::Triangles, 36, gl::GL_UNSIGNED_INT, nullptr, 4);
gl::drawElementsInstancedBaseVertex(..., 100, 4);
gl::drawArraysInstancedBaseInstance(gl::Primitive::Triangles, 0, 3, 100, 0);
gl::multiDrawArrays(gl::Primitive::Triangles, firsts, counts, drawCount);
gl::multiDrawElements(gl::Primitive::Triangles, counts, gl::GL_UNSIGNED_INT, indexPtrs, drawCount);
gl::multiDrawElementsBaseVertex(...);
gl::multiDrawArraysIndirect(gl::Primitive::Triangles, indirectPtr, drawCount, stride);
gl::multiDrawElementsIndirect(gl::Primitive::Triangles, gl::GL_UNSIGNED_INT, indirectPtr, drawCount, stride);
```

## Debug labels & groups (`gl/debug.hpp`, GL 4.3 / KHR_debug)

```cpp
fuhgl::gl::objectLabel(fuhgl::gl::ObjectType::Buffer, vbo.id(), "instanceVBO"); // visible in RenderDoc/NSight
std::string name = fuhgl::gl::getObjectLabel(fuhgl::gl::ObjectType::Buffer, vbo.id());
fuhgl::gl::pushDebugGroup(fuhgl::gl::GL_DEBUG_SOURCE_APPLICATION, 1, "render opaque");
// ... draw calls ...
fuhgl::gl::popDebugGroup();
fuhgl::gl::debugMessageInsert(fuhgl::gl::GL_DEBUG_SOURCE_APPLICATION, fuhgl::gl::GL_DEBUG_TYPE_OTHER, 1,
                              fuhgl::gl::GL_DEBUG_SEVERITY_LOW, "checkpoint");
fuhgl::gl::debugMessageControlAll(false); // silence everything
// message polling: gl::getDebugMessageLog(...); callback-based output: gl::setDebugCallback (tutorial in USER_GUIDE)
```

## Getters, queries, sync, internal formats, image ops

```cpp
gl::isBuffer(id); gl::isTexture(id); gl::isProgram(id); ...   // glIsXxx
gl::isEnabled(gl::Cap::DepthTest); gl::getBoolean(gl::GL_SCISSOR_TEST);
gl::getIntegeri(gl::GL_MAX_TEXTURE_SIZE, 0);                   // glGetIntegeri_v
gl::getQueryi(gl::GL_TIME_ELAPSED, gl::GL_QUERY_COUNTER_BITS); // glGetQueryiv
gl::beginQueryIndexed(gl::GL_TIME_ELAPSED, 0, q.id()); gl::endQueryIndexed(...);
gl::waitSync(sync, 0, gl::GL_TIMEOUT_IGNORED); gl::getSynci(sync, gl::GL_SYNC_STATUS);
gl::getInternalformati(gl::GL_TEXTURE_2D, gl::GL_RGBA8, gl::GL_INTERNALFORMAT_SUPPORTED); // == 1
gl::copyImageSubData(srcTex, gl::GL_TEXTURE_2D, 0, 0,0,0, dstTex, gl::GL_TEXTURE_2D, 0, 4,4,0, w,h,1); // GL 4.3
gl::clearTexImage(tex.id(), 0, gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, green);  // GL 4.4
gl::clearTexSubImage(...);
```

## Test

`tests/gl_smoke_state.cpp` (CI, real context): stencil & blend verified by pixel readback,
polygonMode fill-vs-line, multiDraw/drawRangeElements pixel checks, object-label
round-trip, internalformat queries, copyImageSubData/clearTexImage readback, query
counter bits, fence sync status, FBO layer attach/invalidate. Plus table-presence
checks in `tests/test_loader.cpp`.
