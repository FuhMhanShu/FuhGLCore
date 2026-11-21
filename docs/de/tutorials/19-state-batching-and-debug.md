# 19 — GL-State, Draw-Batching, Debug-Labels & Image-Ops (F13)

> 🇬🇧 **English version:** [docs/en/tutorials/19-state-batching-and-debug.md](../../en/tutorials/19-state-batching-and-debug.md)

**Ziel:** die übrigen alltäglichen GL-Einstiegspunkte als schlanke `fuhgl::gl`-Freifunktionen
(alle in der kuratierten Loader-Tabelle; No-Ops, wenn der Treiber sie nicht hat): Blending/Stencil,
Rasterizer-State, Viewport-/Scissor-Arrays, gebatchte Draws, Objekt-Labels & Debug-Gruppen,
Query-/Sync-Helfer, `glIsXxx`/`glGet*`-Getter, Internal-Format-Abfragen und GPU-Image-Kopien/-Clears.

## State (Namespace `fuhgl::gl`, Header `gl/state.hpp`)

```cpp
// Blending / Logic-Op
fuhgl::gl::enable(fuhgl::gl::Cap::Blend);
fuhgl::gl::blendEquation(fuhgl::gl::BlendEquation::FuncAdd);
fuhgl::gl::blendEquationSeparate(fuhgl::gl::BlendEquation::FuncAdd, fuhgl::gl::BlendEquation::Max);
fuhgl::gl::blendFuncSeparate(fuhgl::gl::BlendFactor::SrcAlpha, fuhgl::gl::BlendFactor::OneMinusSrcAlpha,
                             fuhgl::gl::BlendFactor::One, fuhgl::gl::BlendFactor::OneMinusSrcAlpha);
fuhgl::gl::blendColor(0, 0, 0, 1);
fuhgl::gl::logicOp(fuhgl::gl::LogicOp::Xor);              // braucht Cap::Blend? nein — Logic-Op ist ein eigener Pfad
fuhgl::gl::enablei(fuhgl::gl::Cap::Blend, 0);            // pro Draw-Buffer (GL 4.0)
fuhgl::gl::colorMaski(0, true, true, true, true);

// Stencil
fuhgl::gl::enable(fuhgl::gl::Cap::StencilTest);
fuhgl::gl::stencilFunc(fuhgl::gl::CompareFunc::Always, 1, 0xFF);
fuhgl::gl::stencilOp(fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Keep, fuhgl::gl::StencilOp::Replace);
fuhgl::gl::stencilMask(0xFF);
fuhgl::gl::stencilFuncSeparate(fuhgl::gl::Face::Front, ...); // Face-Varianten

// Rasterizer / Viewport / Scissor
fuhgl::gl::polygonMode(fuhgl::gl::Face::FrontAndBack, fuhgl::gl::PolyMode::Line); // Wireframe
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
fuhgl::gl::patchParameteri(fuhgl::gl::GL_PATCH_VERTICES, 3); // Tessellierung, GL_PATCHES-Primitive
```

`Primitive` hat `Patches` dazugewonnen (`GL_PATCHES`).

## Framebuffer-Helfer (`Framebuffer`-Methoden + Freifunktionen)

```cpp
fbo.attachTexture(Framebuffer::Attachment::Color0, arrayTex);        // ganze Array/Cube (glFramebufferTexture)
fbo.attachTextureLayer(Framebuffer::Attachment::Color0, arrayTex, 0, 2); // eine Ebene
int layer = fbo.attachmentParameteri(gl::GL_COLOR_ATTACHMENT0, gl::GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER);
fbo.invalidate(1, &gl::GL_COLOR_ATTACHMENT0);                        // Tiler-GPUs: Resolve überspringen
fbo.invalidateSub(1, &att, x, y, w, h);
gl::drawBuffer(gl::GL_COLOR_ATTACHMENT0);
gl::clearBufferfi(gl::GL_DEPTH_STENCIL, 0, 1.0f, 0);
gl::clearBufferuiv(gl::GL_COLOR, 0, u32);
```

## Draw-Batching

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

## Debug-Labels & -Gruppen (`gl/debug.hpp`, GL 4.3 / KHR_debug)

```cpp
fuhgl::gl::objectLabel(fuhgl::gl::ObjectType::Buffer, vbo.id(), "instanceVBO"); // sichtbar in RenderDoc/NSight
std::string name = fuhgl::gl::getObjectLabel(fuhgl::gl::ObjectType::Buffer, vbo.id());
fuhgl::gl::pushDebugGroup(fuhgl::gl::GL_DEBUG_SOURCE_APPLICATION, 1, "render opaque");
// ... Draw-Calls ...
fuhgl::gl::popDebugGroup();
fuhgl::gl::debugMessageInsert(fuhgl::gl::GL_DEBUG_SOURCE_APPLICATION, fuhgl::gl::GL_DEBUG_TYPE_OTHER, 1,
                              fuhgl::gl::GL_DEBUG_SEVERITY_LOW, "checkpoint");
fuhgl::gl::debugMessageControlAll(false); // alles stummschalten
// Message-Polling: gl::getDebugMessageLog(...); callback-basierte Ausgabe: gl::setDebugCallback (siehe USER_GUIDE)
```

## Getter, Queries, Sync, Internal Formats, Image-Ops

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

`tests/gl_smoke_state.cpp` (CI, echter Kontext): Stencil & Blend per Pixel-Readback
verifiziert, polygonMode fill-vs-line, multiDraw/drawRangeElements-Pixelprüfungen,
Objekt-Label-Roundtrip, Internalformat-Abfragen, copyImageSubData/clearTexImage-Readback,
Query-Counter-Bits, Fence-Sync-Status, FBO-Layer-Attach/Invalidate. Plus
Tabellen-Präsenzprüfungen in `tests/test_loader.cpp`.
