# FuhGLCore — Requirements

Status: final (agreed with maintainer). Language of this document: German.
Code, comments and API docs are English.

## 1. Goal

FuhGLCore is a lightweight, modern C++17 OpenGL wrapper library with **zero
third-party dependencies** (only the C++ standard library and platform system
APIs). It provides:

- a runtime OpenGL function loader (no GLAD/GLEW, no Khronos headers),
- thin RAII wrappers for GL objects (PIMPL idiom),
- a native window + OpenGL context per platform (no GLFW/SDL in the library),
- a small header-only math module (vectors/matrices, column-major, OpenGL layout),
- camera controllers (first person, free 6-DOF, isometric).

## 2. Non-goals

- No widget/GUI toolkit, no scene graph, no asset loading, no audio.
- No Wayland in v1 (Linux v1 = X11/GLX; Wayland/EGL may follow as an optional
  module loaded via `dlopen`).
- No math library dependency (no glm) — math is implemented in-house.
- No exceptions in the API; failures are reported via `bool` / info-log strings.

## 3. Functional requirements

| ID | Requirement | Details |
|----|-------------|---------|
| F1 | Function loader | `fuhgl::gl::init()` after a current context; curated core table GL 1.0–4.6 loaded by name; `fuhgl::gl::procAddress(name)` for anything else; version/extension parsing |
| F2 | Error handling | `glGetError` wrapper, debug output (GL 4.3 / `KHR_debug`) with user callback, debug assert macro; **no exceptions** |
| F3 | RAII GL wrappers | `Shader`, `Program` (uniform location cache + typed setters), `Buffer` (VBO/IBO/UBO/SSBO, map/subdata), `VertexArray`, `Texture` (2D), `Framebuffer`, `Renderbuffer`; PIMPL, movable, non-copyable |
| F4 | State helpers | thin free functions: viewport, clear, blend, depth, scissor, draw calls — no global state manager |
| F5 | Native window + context | `fuhgl::Window`: Win32/WGL, X11/GLX (via `dlopen`, no link dependency), Cocoa/ObjC++ on macOS; creates 3.3 core-profile context by default, calls `gl::init()` internally; event callbacks (resize, key, mouse button, cursor pos, scroll, close); `pollEvents()`, `swapBuffers()`, `shouldClose()` |
| F6 | Math module | `fuhgl::math`: `Vec2/Vec3/Vec4`, `Mat3/Mat4` (column-major, `data()` compatible with `glUniformMatrix*fv`), dot/cross/normalize/lerp, mat mul/transpose/inverse/determinant, `translate/rotate/scale`, `perspective`, `ortho`, `lookAt`, angle helpers; `constexpr`/`noexcept` where possible; header-only |
| F7 | Camera controllers | common base `Camera` (`view()`, `projection(aspect)`, `update(dt)`); `FirstPersonCamera` (yaw/pitch, WASD + mouse look, no roll), `FreeCamera` (6-DOF, roll via Q/E), `IsometricCamera` (orthographic, fixed 45°/≈35.264° angles, pan + zoom); input via `Window` events (`attach(window)`) |
| F8 | Device info & capabilities | `gl::DeviceInfo` + `gl::queryDeviceInfo()` after `init()`: GPU strings (vendor/renderer/GL version/GLSL), parsed `Version`, core-profile flag, full extension list (`gl::extensions()` / `hasExtension()`), curated `Limits` (textures 2D/3D/cube/array, renderbuffer, viewport, attribs, uniforms, texture units, MRT draw buffers/color attachments, MSAA samples, UBO/SSBO bindings+sizes, compute work-group limits, element counts, aniso), `Features` flags (DSA, compute, debug output, anisotropic filtering, SSBO), human-readable `report()`; value types (no PIMPL — no GL handles) |
| F9 | Buffer/texture power-ups + image binding (P1 subset, see §7) | `Buffer`: `mapRange()`/`flushMappedRange()`/`unmap()`, `invalidateData()/invalidateSubData()`, `getSubData()`, parameter query; free `gl::copyBufferSubData(...)`; `VertexArray`: `setAttribI/L()` (integer/64-bit attribs) + GL 4.3 `bindVertexBuffer(s)`/`setAttribFormat(I/L)/Binding/BindingDivisor`; `Texture`: `setSubImage3D`, `copyTexImage2D`, `copyTexSubImage2D/3D`, `compressedImage2D/3D`, `compressedSubImage2D/3D`, `texBuffer(Range)`, `textureView()`, parameter getters, free `gl::getTexImage`/`gl::getCompressedTexImage`; compute/images: `gl::bindImageTexture(s)`, `gl::dispatchComputeIndirect`, `Program::shaderStorageBlockBinding()`/`uniform*uiv`/`getUniform*`; new loader entries + constants |
| F10 | Procedural textures | `render::procedural`: CPU-side RGBA8 generators `checker/gradient/stripes/bricks/rings/noise(fBm, seeded)/voronoi`, deterministic; `makeTexture()` uploads with mipmaps; value types, no GL handles |
| F11 | Predefined materials | `render::materials`: `matte(albedo)`, `textured(tex, tint)`, `emissive(color, strength)`, `texturedEmissive(...)` — preset `Material` (builtinShader flag: albedo/emissive/albedoTexture fields) driven by the pipelines' built-in shaders via uAlbedo/uEmissive/uUseTex/uMatTex (unit 3); deferred stores emissive in a 4th G-buffer attachment; custom-program materials unchanged |
| F12 | Solids & CSG | `render::Solid` abstract body (`inside(p)` + AABB, polymorphic value type via `SolidPtr`); primitives `makeCube/makeSphere/makeCylinder/makeCone/makeTorus` + `translated()`; CSG operators `\|`, `&`, `-` on SolidPtr (union/intersection/difference, derived AABBs); `buildMeshData(res)` = marching tetrahedra (6 tets/voxel, gradient-fixed winding, accumulated vertex normals, shared vertices, planar UVs, standard MeshData layout pos/normal/color/uv) and `buildMesh()` = GL upload; `Mesh::setMeshData()`, shapes::box/quad moved to the same layout; **bounding volumes**: `makeBoundingBox/Sphere/Octahedron` (low-poly) + `makeBoundingVolume(s, BoundingVolume)` + `render::intersects(Frustum, Solid)`; `Frustum::intersectsAabb()` in math |
| F13 | GL state, batching, debug & image ops (P1 remainder, see §7) | ~75 new curated loader entries + constants; `gl/state.hpp` free functions + enums (`BlendEquation`, `LogicOp`, `StencilOp`, `PolyMode`, `ProvokingVertex`, `HintTarget/Mode`, `Primitive::Patches`): blending (incl. per-index 4.0), stencil, polygonMode/offset, line/point, sample coverage/mask, depth/viewport/scissor ranges+arrays (4.1), provoking vertex, primitive restart, hints, patch params; framebuffer: `glDrawBuffer`, `glFramebufferTexture(Layer)`, `glClearBufferuiv/fi`, `glInvalidate(Sub)Framebuffer`, attachment params + `Framebuffer::{attachTexture, attachTextureLayer, invalidate, invalidateSub, attachmentParameteri}`; batching draws (range/baseVertex/instancedBase*/multiDraw(+BaseVertex/Indirect)); `gl/debug.hpp`: message control/insert/log, push/popDebugGroup, object labels (KHR_debug); getters `glIsXxx`, `glGetBooleanv/i_v/Integeri_v/64i_v`, `glGetQueryiv`, indexed queries, `glWaitSync`/`glGetSynciv`; `glGetInternalformativ(i64v)`; `glCopyImageSubData`, `glClearTexImage/SubImage` |

## 4. Non-functional requirements

- **N1 Zero dependencies**: no third-party libraries to build or link; C++17
  standard library only. Windows: Win32 + `opengl32.dll` (loaded at runtime).
  Linux: `dlopen("libX11.so.6")` / `dlopen("libGL.so.1")` — no dev packages needed
  at build time. macOS: Cocoa/OpenGL system frameworks.
- **N2 Performance**: wrappers are thin inline layers with no overhead vs. raw GL;
  no allocations in the render path; PIMPL objects are created only during setup.
- **N3 Style**: compact code, English Doxygen comments, encapsulated classes,
  PIMPL by default for GL resource classes (math/camera types are value types),
  namespace `fuhgl` (`fuhgl::math` for math).
- **N4 Build**: CMake ≥ 3.20, static library `fuhglcore`, `install()` + CMake
  package export (`find_package(FuhGLCore)`), options `FUHGL_BUILD_EXAMPLES`,
  `FUHGL_BUILD_TESTS`.
- **N5 Tests**: unit tests without a GL context (math, cameras, loader name table,
  version parsing) using a minimal built-in test runner — no test framework
  dependency; GL smoke test headless via EGL surfaceless where available;
  window test under Xvfb in CI.
- **N6 Docs**: README (build/usage), Doxygen comments, runnable example
  (rotating triangle in a native window).

## 5. Platforms and GL baseline

- Baseline **OpenGL 3.3 core profile** (available everywhere, incl. macOS ≤ 4.1);
  functions up to 4.6 are loaded opportunistically; DSA (4.5) usable via
  `procAddress` when present.
- Platforms: Windows (WGL), Linux (X11/GLX), macOS (Cocoa). Linux v1: X11 only.

## 7. Backlog / TODO — GL coverage gaps (from coverage analysis, 2026-11-21)

Covered today: 115 core functions in the curated loader table + 23 DSA entries
(GL 4.5) + runtime `procAddress` for a few platform/legacy entries. Below: the
GL core functions **not** covered yet that are worth implementing, grouped by
priority. F9 (in progress) covers the buffer/texture/image-binding part of P1.

### P1 — high value, low effort
- **Buffers**: `glMapBufferRange`, `glFlushMappedBufferRange`, `glCopyBufferSubData`,
  `glGetBufferSubData`, `glGetBufferParameteriv/i64v`, `glInvalidateBufferData`,
  `glInvalidateBufferSubData` → **F9 done**
- **Vertex attribs/binding**: `glVertexAttribIPointer`, `glVertexAttribLPointer`,
  GL 4.3 `glBindVertexBuffer(s)`, `glVertexAttribBinding/Format/IFormat/LFormat`,
  `glVertexBindingDivisor` → **F9 done**
- **Textures**: `glTexSubImage3D`, `glCopyTexImage2D`, `glCopyTexSubImage2D/3D`,
  `glGetTexImage`, `glGetTexLevelParameteriv`, `glGetTexParameter*`,
  `glCompressedTexImage2D/3D`, `glCompressedTexSubImage2D/3D`, `glGetCompressedTexImage`,
  `glTexBuffer(Range)` → **F9 done**
- **Compute/images**: `glBindImageTexture(s)`, `glDispatchComputeIndirect`,
  `glShaderStorageBlockBinding`, `glGetShaderStorageBlockIndex` → **F9 done**
- **Framebuffers**: `glDrawBuffer`, `glFramebufferTexture(Layer)`, `glClearBufferfi/uiv`,
  `glInvalidateFramebuffer/SubFramebuffer`, `glGetFramebufferAttachmentParameteriv` → **F13 done**
- **Blending/stencil**: `glBlendEquation(Separate)`, `glBlendFuncSeparate`, `glBlendColor`,
  `glLogicOp`, `glStencilFunc/Op/Mask(Separate)` → **F13 done**
- **Rasterizer**: `glPolygonMode/Offset`, `glLineWidth`, `glPointSize`, `glSampleCoverage`,
  `glSampleMaski`, `glMinSampleShading`, `glDepthRange(f/Arrayv)`, `glViewportArrayv/Indexedf`,
  `glScissorArrayv/Indexed`, `glProvokingVertex`, `glPrimitiveRestartIndex`, `glHint` → **F13 done**
- **Draw**: `glDrawRangeElements`, `glMultiDrawArrays/Elements(+Indirect)`,
  `glMultiDrawElementsBaseVertex`, `glDrawElementsInstancedBaseVertex(BaseInstance)`,
  `glDrawArraysInstancedBaseInstance`, `glPatchParameteri/fv` + `GL_PATCHES` → **F13 done**
- **Uniforms**: `glGetUniformiv/fv/uiv`, `glUniform1ui..4ui(v)` → **F9 done**
- **Reflection**: `glGetActiveUniform`, `glGetActiveUniformBlockiv`, `glGetProgramResource*`,
  `glValidateProgram`, `glProgramParameteri` — TODO
- **Debug**: `glDebugMessageControl/Insert`, `glGetDebugMessageLog`, `glPush/PopDebugGroup`,
  `glObjectLabel/glGetObjectLabel` → **F13 done**
- **Queries/sync/getters**: `glGetQueryiv`, `glBegin/EndQueryIndexed`, `glWaitSync`,
  `glGetSynciv`, `glGetBooleanv/i_v`, `glIsEnabled`, `glIsBuffer/Texture/...` → **F13 done**
- **Formats**: `glGetInternalformativ/i64v` → **F13 done**
- **Images 4.3/4.4**: `glCopyImageSubData`, `glTextureView`, `glClearTexImage/SubImage` → **F13 done**

### P2 — complete the DSA table (GL 4.5, mechanical via FUHGL_DSA macro)
`glNamedBufferStorage`, `glMapNamedBuffer(Range)`, `glUnmapNamedBuffer`,
`glFlushMappedNamedBufferRange`, `glCopyNamedBufferSubData`, `glClearNamedBufferSubData`,
`glInvalidateNamedBufferData/SubData`, `glGetNamedBufferParameteriv/i64v`,
`glGetNamedBufferSubData`, `glCreateTransformFeedbacks`, `glNamedFramebufferDrawBuffer(s)`,
`glNamedFramebufferReadBuffer`, `glClearNamedFramebuffer*`, `glNamedFramebufferTextureLayer`,
`glNamedRenderbufferStorageMultisample`, `glGetNamedRenderbufferParameteriv`,
`glTextureParameteri/f/fv`, `glTextureSubImage3D`, `glCompressedTextureSubImage2D/3D`,
`glCopyTextureSubImage2D/3D`, `glGenerateTextureMipmap`, `glTextureStorage2DMultisample/3DMultisample`,
`glGetTextureImage`, `glGetTextureLevelParameteriv`, `glGetCompressedTextureImage`,
`glVertexArrayAttribBinding/IFormat/LFormat`, `glVertexArrayBindingDivisor`, `glGetVertexArray*`,
`glSamplerParameterf/fv`, `glBindSamplers`, `glCreateQueries`, `glBindImageTexture`,
`glTextureBuffer(Range)`, `glClearTexImage/SubImage` — TODO

### P3 — larger subsystems
- Program pipelines / separable programs (4.1): `glCreateProgramPipelines`,
  `glBindProgramPipeline`, `glUseProgramStages`, `glActiveShaderProgram`, `glProgramUniform*`,
  `glCreateShaderProgramv` — TODO
- Transform feedback: `glBindTransformFeedback`, `glBegin/End/Pause/ResumeTransformFeedback`,
  `glDrawTransformFeedback(Stream)`, `glTransformFeedbackVaryings` — TODO
- Tessellation: `GL_PATCHES` primitive + `glPatchParameteri/fv` (shader stages covered) — TODO
- Shader cache: `glGetProgramBinary`, `glProgramBinary`, `glShaderBinary`,
  `glReleaseShaderCompiler`, `glGetShaderPrecisionFormat` — TODO
- Shader subroutines (4.0): `glGetSubroutineUniformLocation`, `glUniformSubroutinesuiv`, ... — TODO (low value)
- GL 4.6: `glMultiDrawArraysIndirectCount`, `glMultiDrawElementsIndirectCount`,
  `glPolygonOffsetClamp`, `glSpecializeShader` — TODO

## 6. Feasibility assessment

Feasible with low-to-medium risk. Estimated MVP size ≈ 5–6k lines:
loader ~400, GL wrappers ~2k, window ~1.2k (3 platforms), math ~800,
cameras ~400, CMake/docs/tests/example ~1k. Known risks: Windows > 1.1
functions need `wglGetProcAddress` with a current context (handled by `Window`);
Wayland out of scope for v1; only Linux is compile-verifiable in the dev sandbox
(Windows/macOS verified via CI matrix). Math + cameras are fully unit-testable
headless.
