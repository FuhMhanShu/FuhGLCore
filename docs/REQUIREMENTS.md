# FuhGLCore — Requirements

Status: final (agreed with maintainer). Language of this document: English.
Code, comments and API docs are English.

## 1. Goal

FuhGLCore is a lightweight, modern C++17 OpenGL wrapper library with **zero
third-party dependencies** (only the C++ standard library and platform system
APIs). It provides:

- a runtime OpenGL function loader (no GLAD/GLEW, no Khronos headers),
- thin RAII wrappers for GL objects (PIMPL idiom),
- a native window + OpenGL context per platform (no GLFW/SDL in the library),
- a small header-only math module (vectors/matrices, column-major, OpenGL layout),
- camera controllers (first person, free 6-DOF, isometric),
- a built-in render layer on top of the core (see §3, F12–F25): forward + deferred
  pipelines and ready-to-use default renderers (lights, shadow mapping, SSAO, HDR,
  bloom, god rays, parallax/bump mapping, fog), procedural textures & materials,
  CSG solids, text/FPS overlay, wireframe overlay and instancing.

The name "Core" refers to the zero-dependency GL/window/math core; the render layer
is a deliberately small, optional-on-top module (include `fuhgl/render.hpp`) — not a
separate engine project.

## 2. Non-goals

- No widget/GUI toolkit, no scene graph, no asset loading, no audio.
- Linux default window backend is X11/GLX. An **experimental** EGL backend (incl.
  Wayland surfaces and headless `surfaceless`) exists as a build option
  (`-DFUHGL_LINUX_BACKEND=EGL`, used by CI for headless rendering) — it is not part
  of the v1 stability promise.
- No math library dependency (no glm) — math is implemented in-house.
- No exceptions are thrown by the API for domain errors; failures are reported via
  `bool` / info-log strings. (The C++ standard library itself may still throw, e.g.
  `std::bad_alloc` on allocation failure — the library code never throws.)

## 3. Functional requirements

| ID | Requirement | Details |
|----|-------------|---------|
| F1 | Function loader | `fuhgl::gl::init()` after a current context; curated core table GL 1.0–4.6 loaded by name (platform-dependent: macOS caps at GL 4.1, Windows/Linux up to 4.6); `fuhgl::gl::procAddress(name)` for anything else; version/extension parsing |
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
| F14 | Light sources (directional/point/spot) | `render::Light` value type in `render/light.hpp`: factories `Light::point/directional/spot(...)`, fields for color/intensity/attenuation (c/l/q)/range/spot cones, CPU math `attenuation(d)`/`coneFactor(toLight)` mirroring the shader; `render::LightBlock` = C++ mirror of the extended std140 UBO (posIntensity/color/dirType/params0/params1, 64 lights) + `packLightBlock()`; shared GLSL (`lighting_glsl.hpp`) computes per-light direction+attenuation+cone; both pipelines upload the block per frame; `SceneView::lights` is now `std::vector<Light>` (aggregate init still = point light); tests: `test_light.cpp`, `gl_smoke_lights.cpp`; example `lights`; tutorial 20 |
| F15 | Shadow mapping (depth pass + light VP + PCF) | `render::ShadowMap` (PIMPL: depth texture `GL_DEPTH_COMPONENT24` LINEAR + depth-only FBO with `glDrawBuffer(GL_NONE)`, `render(scene, lightVP)` draws back faces to reduce acne); free helpers `lightView/lightProjection/lightViewProj(light, center, radius)` (directional = ortho box around scene sphere with safe up-vector, spot = perspective fov 2*coneOuter); `SceneView::{shadowMap, lightVP, shadowLightIndex, shadowStrength}`; shared GLSL `kShadowUniforms`/`kShadowFunc` (slope-scaled bias + 3x3 PCF over LINEAR depth) applied in `shade()` only to the shadow light; both pipelines bind the map on unit 4 (1x1 dummy when off); `GL_NONE` constant; tests: `test_shadow.cpp` (light VP math), `gl_smoke_shadows.cpp` (with/without shadow pixel diff); example `shadows`; tutorial 21 |
| F16 | Default forward renderer + fog | `render::ForwardRenderer` (PIMPL) in `render/forward_renderer.hpp`: owns `ForwardPipeline` + `ShadowMap` + optional own RGBA8/depth target (`create(w,h)` / `target()` / `blitToScreen()` / `resize()`); scene config `addLight/setLights/setAmbient/setClearColor/setFog`; `enableShadows(size)` + `setShadowSceneBounds(center,radius)` + `setShadowStrength`; `render(camera, items, w, h)` = auto shadow depth pass from the first directional/spot light (`lightViewProj`) + forward pass; `SceneView::{fogColor, fogDensity}` + exponential fog in both pipeline shaders (`exp(-density*d)`); tests: `gl_smoke_forward.cpp` (auto-shadow blob + fog color shift); example `forward` (+2 CI screenshots); tutorial 22 |
| F17 | Default deferred renderer + post FX | `render::DeferredRenderer` (PIMPL): `DeferredPipeline` split into public `renderGBuffer()`/`renderLighting(scene, target)` passes + G-buffer texture accessors; G-buffer shader: parallax occlusion mapping (height map unit 5, derivative tangent frame, 64-step ray march) + bump/normal mapping (unit 4) via new `Material` fields (`normalMap`, `heightMap`, `parallaxScale`, `parallaxLayers`) + presets `materials::normalMapped/parallax`; `procedural::normalFromHeight/makeNormalMap`; post chain in `deferred_renderer.cpp`: SSAO (deterministic LCG kernel injected into GLSL, world-space hemisphere + depth compare, separable blur, unit 5 in lighting pass) → HDR lighting into RGBA16F (sun disk in sky via `SceneView::sun*`, `uInvViewProj`, AO multiply, fog, shadows) → bloom (bright-pass threshold + gaussian ping-pong) → crepuscular rays (radial blur toward `sunScreenUV(camera, sunDir, w, h)`) → composite (ACES + gamma into LDR `output()`); shadows like ForwardRenderer (`enableShadows`/`setShadowSceneBounds`); tests: `gl_smoke_deferred_fx.cpp` (per-effect pixel toggles), `sunScreenUV` in `test_shadow.cpp`; example `postfx` (+2 CI screenshots); tutorial 23 |
| F18 | Complete GL 4.5 DSA table (P2) | `gl::DsaFunctions` extended from 22 to 72 typed entries (buffers: `glNamedBufferStorage`, `glMapNamedBuffer(Range)`, `glUnmapNamedBuffer`, `glFlushMappedNamedBufferRange`, `glCopyNamedBufferSubData`, `glClearNamedBufferSubData`, `glInvalidateNamedBufferData/SubData`, `glGetNamedBufferParameteriv/i64v`, `glGetNamedBufferSubData`; TF: `glCreateTransformFeedbacks`; FBOs: `glNamedFramebufferDrawBuffer(s)`, `glNamedFramebufferReadBuffer`, `glClearNamedFramebufferiv/uiv/fv/fi`, `glNamedFramebufferTextureLayer`; renderbuffers: `glNamedRenderbufferStorageMultisample`, `glGetNamedRenderbufferParameteriv`; textures: `glTextureParameteri/f/fv`, `glTextureSubImage3D`, `glCompressedTextureSubImage2D/3D`, `glCopyTextureSubImage2D/3D`, `glGenerateTextureMipmap`, `glTextureStorage2D/3DMultisample`, `glGetTextureImage`, `glGetCompressedTextureImage`, `glGetTextureLevelParameteriv`, `glTextureBuffer(Range)`; VAOs: `glVertexArrayAttribBinding/IFormat/LFormat`, `glVertexArrayBindingDivisor`, `glGetVertexArrayiv/Indexediv/Indexed64iv`; samplers: `glSamplerParameterf/fv`, `glBindSamplers`; queries: `glCreateQueries`) via `FUHGL_DSA` in `src/gl/loader.cpp`; new `gl::dsaFunctionNames()`/`dsaFunctionNameCount()`; new constants in `include/fuhgl/gl/types.hpp`; tests: `test_loader.cpp` (72-entry presence list), `gl_smoke_dsa.cpp` (readback-verified buffer/texture/sampler/VAO/FBO/query/TF ops, skips without GL 4.5); tutorial 11 addendum |
| F19 | Text rendering + FPS overlay + screenshot fixes | `render::TextRenderer` (PIMPL, `render/text.hpp`): embedded public-domain 8x8 font atlas (font8x8, IBM VGA based, 128x64 GL_R8 texture), batched glyph quads (one draw call per begin/end), screen-space pixel coords (y down), alpha blending; `render::FpsCounter` value type (EMA fps); `render::font8x8Glyph()` pure-data accessor; `TextRenderer::measure/lineHeight`; example `examples/text` (+ CI screenshot `tutorial-text.png`); `examples/gpuinfo` now labels its capability bars with TextRenderer (tutorial 14 screenshot readable again); fixed broken `](images/...)` links in tutorials 20-23 (needed `../images/`); tests: `test_text.cpp` (font/measure/FpsCounter), `gl_smoke_text.cpp` (readback-verified text pixels); tutorial 24 |
| F20 | Wireframe rendering (solids + bounding boxes as line grids) | `render::Wireframe` (PIMPL, `render/wireframe.hpp`): CPU-side line queue (`addLine`/`addBox`/`addBoundingBox`/`addMesh`/`addSolid`), one batched `glDrawArrays(GL_LINES)` per `render(camera,w,h)` with depth test; pure helpers `boxCorners()`, `kBoxEdgeIndices`, `wireframeEdges()` (deduped triangle edges) unit-testable without GL; `examples/solids --wireframe` (+ CI screenshot `tutorial-wireframe.png`); tests: `test_wireframe.cpp`, `gl_smoke_wireframe.cpp` (readback-verified line pixels); tutorial 25 |
| F21 | P3: program pipelines, transform feedback, binaries, subroutines, GL 4.6 | 67 new curated loader entries (program pipelines + all glProgramUniform* + glCreateShaderProgramv + glProgramParameteri; TF: gen/bind/begin/end/pause/resume/draw/varyings/queries; binaries: glGetProgramBinary/glProgramBinary/glShaderBinary/glReleaseShaderCompiler/glGetShaderPrecisionFormat; subroutines: index/location/uniform queries; 4.6: glSpecializeShader/glPolygonOffsetClamp/glMultiDraw*IndirectCount); new wrappers `gl::ProgramPipeline` (handle class: use/useStages/setActiveShaderProgram/programUniform/validate/createShaderProgram) and `gl::TransformFeedback` + `Program::setTransformFeedbackVaryings/setParameteri`; new constants (stage bits, TF, binary, subroutine, 4.6) in types.hpp; tests: `test_loader.cpp` f21[] presence list, `gl_smoke_p3.cpp` (pipeline pixel check, TF capture readback, binary round-trip, subroutine round-trip; skips < 4.1); tutorial 26 |
| F22 | Program reflection (uniforms / attributes / uniform blocks / resources) | 12 new curated loader entries (`glGetActiveAttrib/Uniform/UniformName/Uniformsiv`, `glGetActiveUniformBlockiv/Name`, `glGetUniformIndices`, `glGetProgramInterfaceiv`, `glGetProgramResourceiv/Name/Index`, `glGetAttachedShaders`) + reflection constants (`GL_ACTIVE_*`, `GL_UNIFORM_*`, `GL_UNIFORM_BLOCK_*`, `GL_*` resource interfaces/props, shader var types) in types.hpp; new API on `Program`: `activeUniforms()` -> `UniformInfo{name,type,size,blockIndex,offset,arrayStride}`, `activeAttributes()` -> `AttributeInfo{name,type,size,location}`, `uniformBlocks()` -> `UniformBlockInfo{name,index,dataSize,binding,activeUniforms,uniformIndices}`; tests: `test_loader.cpp` f22[] list, `gl_smoke_reflection.cpp` (headless reflect of UBO+uniforms+attribs, verified); tutorial 27 |
| F23 | Textured solids (UV mapping modes) | `render::SolidUv` enum (`PlanarXZ` default = old behavior, `PlanarXY`, `PlanarYZ`, `Spherical` equirectangular around AABB center, `Cylindrical` around Y) + pure `render::solidUvPoint(mode, p, mn, mx)` in `render/solid.hpp`; `Solid::buildMeshData/buildMesh` take an optional `SolidUv` (default `PlanarXZ`, backward compatible); meshed UVs always land in [0,1]; combines with `materials::textured()` (albedo on unit 3) + `procedural::checker/bricks/voronoi`; `examples/solids --textured` shows one body per mode (+ CI screenshot `tutorial-solids-textured.png`); tests: UV unit tests in `test_solids.cpp` (corner/center/pole/seam values, degenerate axes, all-mode [0,1] + variation checks, default-mode equivalence), `gl_smoke_solids.cpp` extended with a spherical-UV textured body + GL error check; tutorial 18 addendum |
| F24 | Instanced rendering (per-instance model matrices) | `Mesh::setInstanceData(models, count, usage)` uploads a divisor-1 mat4 attribute (locations 4..7, 64B stride) into the mesh VAO; `Mesh::updateInstanceData()` re-uploads via `glBufferSubData`; `Mesh::clearInstanceData()` disables attribs 4..7; `Mesh::instanceCount()`; `Mesh::draw()` auto-issues `glDrawElementsInstanced`/`glDrawArraysInstanced` when instances are set (functions already loaded since F13); builtin pipeline shaders (forward VS, deferred G-buffer VS, shadow depth VS) declare `layout(location=4) in mat4 aInstanceModel` + `uniform int uInstanced` and use `uModel * aInstanceModel` when set (set automatically in `detail::bindItem` and the shadow pass from `instanceCount()` — no separate program variants; uninstanced draws unaffected); example `examples/instancing` (324 trees + 96 rocks in 2 draw calls, animated via `updateInstanceData`, + CI screenshot `tutorial-instancing.png`); test: `gl_smoke_instancing.cpp` (3-instance box pixel probes, update/clear behavior, GL-error checked); tutorial 28 |

| F25 | README showcase images (DeferredRenderer landscape) | new example `examples/showcase`: small landscape (textured ground with repeated procedural noise UVs, 12 CSG trees, 8 CSG rocks, 1 CSG monolith — one mesh each, placed via model matrices) rendered with `render::DeferredRenderer` + `enableShadows(2048)` + SSAO + HDR sun disk + bloom + crepuscular god rays + fog + gradient sky + TextRenderer HUD; `--mode day|sunset` (sun lowered into the frustum for god rays); two CI screenshot lines produce `docs/images/readme-showcase-day.png` + `readme-showcase-sunset.png` (960x540), referenced at the top of `README.md`; tutorial 29; no new library code, no new tests (covered by existing gl_smoke_deferred_fx) |
| F26 | Review hygiene (docs/CI/API consistency, no functional changes) | (a) test semantics: ctest labels `unit` (no GL) vs `gl_context`, `SKIP_RETURN_CODE 77` → context-less runs report "Skipped" instead of silently passing, driver-capability gaps print `SKIP (driver capability): …`, ci.yml runs the two label sets as separate named steps and the Xvfb smoke accepts only rc 0/124; (b) REQUIREMENTS aligned with reality: document language line fixed (English), §1 goal includes the render layer, EGL/Wayland declared experimental build option, exception claim präzisiert, F1 notes macOS 4.1 cap; (c) structure/naming: `ForwardPipeline` moved to its own header `render/forward_pipeline.hpp` (pipeline.hpp = base class only), Pipeline-vs-Renderer roles documented in render headers, "Core" naming explained in README; (d) API: central texture-unit constants `render::kTextureUnit*` replacing magic numbers, `ForwardPipeline::create(w,h)` signature aligned with `DeferredPipeline` (size optional, only used for owned targets), empty `ForwardPipeline::Impl` PIMPL removed (plain handle-owning class), README loader claim qualified per-platform; (e) release hygiene: `CHANGELOG.md` started, tag `v0.1.0`, repo-description typo fixed |

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

Covered today: 115+ core functions in the curated loader table + 72 DSA entries
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
  `glValidateProgram`, `glProgramParameteri` — **Reflection done (F22)**
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
`glTextureBuffer(Range)`, `glClearTexImage/SubImage` — **P2 done (F18)**: all 50 remaining
entries are now typed fields in `gl::DsaFunctions` + `FUHGL_DSA` rows in `kDsaEntries`
(72 DSA entries total; `glBindImageTexture`/`glClearTexImage/SubImage` were already in the
curated table via F13); new `gl::dsaFunctionNames()`/`dsaFunctionNameCount()` accessors;
new constants in `types.hpp` (storage bits, `GL_TEXTURE_BUFFER_*`, sRGB/RG/BGRA formats,
query targets, `GL_ELEMENT_ARRAY_BUFFER_BINDING`, `GL_SAMPLER_BINDING`, ...);
tests: `test_loader.cpp` P2 presence list (72 entries), `gl_smoke_dsa.cpp` (buffer
mapping/copy/clear round-trips, texture 2D/3D/MS/buffer uploads + readback, sampler
binding, VAO formats/divisors/queries, FBO typed clears + texture layer + MS blit,
queries, transform feedback); tutorial 11 addendum

### P3 — larger subsystems — **P3 done (F21)**
- Program pipelines / separable programs (4.1): `glCreateProgramPipelines`,
  `glBindProgramPipeline`, `glUseProgramStages`, `glActiveShaderProgram`, `glProgramUniform*`,
  `glCreateShaderProgramv` — **P3 done**
- Transform feedback: `glBindTransformFeedback`, `glBegin/End/Pause/ResumeTransformFeedback`,
  `glDrawTransformFeedback(Stream)`, `glTransformFeedbackVaryings` — **P3 done**
- Tessellation: `GL_PATCHES` primitive + `glPatchParameteri/fv` (shader stages covered) — **P3 done** (glPatchParameteri/fv in F13, GL_PATCHES)
- Shader cache: `glGetProgramBinary`, `glProgramBinary`, `glShaderBinary`,
  `glReleaseShaderCompiler`, `glGetShaderPrecisionFormat` — **P3 done**
- Shader subroutines (4.0): `glGetSubroutineUniformLocation`, `glUniformSubroutinesuiv`, ... — **P3 done**
- GL 4.6: `glMultiDrawArraysIndirectCount`, `glMultiDrawElementsIndirectCount`,
  `glPolygonOffsetClamp`, `glSpecializeShader` — **P3 done**

## 6. Feasibility assessment

Feasible with low-to-medium risk. Estimated MVP size ≈ 5–6k lines:
loader ~400, GL wrappers ~2k, window ~1.2k (3 platforms), math ~800,
cameras ~400, CMake/docs/tests/example ~1k. Known risks: Windows > 1.1
functions need `wglGetProcAddress` with a current context (handled by `Window`);
Wayland out of scope for v1; only Linux is compile-verifiable in the dev sandbox
(Windows/macOS verified via CI matrix). Math + cameras are fully unit-testable
headless.
