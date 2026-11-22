# Changelog

All notable changes to FuhGLCore are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/) (the CMake project version is
`0.1.0` with same-major compatibility).

## [Unreleased]

- qmake support so FuhGLCore can be consumed as a dependency from Qt/qmake projects:
  `fuhglcore.pro` builds the standalone static library (`qmake6 fuhglcore.pro && make`),
  `fuhglcore.pri` is the include-as-dependency file (`include($$FUHGLCORE_DIR/fuhglcore.pri)`
  compiles the sources into the consumer; `FUHGLCORE_DIR` via qmake var or environment).
  Both mirror `CMakeLists.txt` (source lists, platform backends, C++17, `-Wall -Wextra
  -Wpedantic`, `FUHGL_LINUX_BACKEND=X11|EGL`). CI smoke-builds both on Linux (`qt6-qmake`),
  including a minimal consumer app. Documented in README.md / README.de.md / DEVELOPMENT.md;
  REQUIREMENTS.md N4 updated.
- README reworked: the "lightweight OpenGL wrapper" headline is replaced by a
  description of the current scope (zero-dependency GL core + built-in render
  layer); complete German translation added as `README.de.md` (cross-linked).
- All 29 tutorials translated into German (`docs/de/tutorials/`, same file names)
  following the InMemDB docs convention: English tutorials moved to
  `docs/en/tutorials/`, new `docs/README.md` language picker, per-file
  cross-language links at the top of each tutorial. All links updated
  (README.md, README.de.md, USER_GUIDE.md, DEVELOPMENT.md).
- Project hardening:
  - CI: top-level `permissions: contents: read`, `concurrency` with
    `cancel-in-progress`, all actions pinned to commit SHAs
    (`actions/checkout` v4.2.2, `actions/upload-artifact` v4.6.2), new `hardening`
    job (strict `-Werror` build + ASan/UBSan unit tests with leak detection).
  - Build: new option `FUHGL_ENABLE_HARDENING=ON` (default, GNU/Clang) — stack
    protector, stack-clash protection, `_FORTIFY_SOURCE=2` (optimized configs),
    full RELRO + noexec stack on Linux, each guarded by a feature check.
  - New `SECURITY.md` with private vulnerability reporting.
  - `Shader::compile(nullptr)` is now safe (treated as empty source) and
    `Shader::resolveIncludes()` caps the resolved output at 4 MiB in addition to
    the include-depth limit (16); covered by new unit tests.
  - Fixed two warnings (`-Werror`-clean now): unused variable in
    `src/render/deferred.cpp`, unused `ext` in `src/render/solid.cpp`.

### Added
- F27 — `fuhgl::math::Quat` (`math/quat.hpp`): quaternions for rotations —
  `fromAxisAngle` / `fromEuler` / `lookRotation` / `fromMat3`, `toMat3` / `toMat4`
  (`toMat4(q)` matches `rotate(angle, axis)`), `slerp` / `nlerp`, `inverse`,
  `angle` / `toAxisAngle`; and `fuhgl::math::Noise` (`math/noise.hpp`): deterministic
  seeded improved-Perlin 2D/3D + fBm (amplitude-normalized). Header-only value types,
  unit tests (`test_quat.cpp`, `test_noise.cpp`), tutorials 30 (en + de).

### Added
- F28 — PBR (metallic-roughness, Cook-Torrance GGX) in the built-in forward + deferred
  pipelines: `Material::metallic/roughness/ao`, presets `materials::pbr()` /
  `pbrTextured()`, 5th G-buffer attachment (unit `kTextureUnitGBufferPbr`), shared GLSL
  `kPbrFuncs` + new `shade()` in `lighting_glsl.hpp`.
- F29 — IBL: `render::Environment` (`render/environment.hpp`) bakes an irradiance
  cubemap, a GGX-prefiltered env cubemap (mip per roughness) and a BRDF LUT from any
  source cubemap on the CPU (deterministic, GL 3.3-safe); `SceneView::env` /
  `ForwardRenderer::setEnvironment()`; texture units 7/8/9; `kEnvUniforms` +
  `pbrAmbient()` in GLSL. Tests: `test_environment.cpp` (unit), `gl_smoke_pbr.cpp`
  (gl_context); tutorials 31 + 32 (en + de).

### Added
- F30 — cascaded shadow maps: `render::CascadeShadowMap` (depth texture array, one layer
  per cascade via `glFramebufferTextureLayer`), `cascadeSplits()` (practical log+uniform
  split) + `cascadeViewProj()` (per-slice fitted ortho box for directional lights);
  `SceneView::cascades`; pipelines sample `sampler2DArray` with per-cascade PCF and
  view-depth cascade selection; `ForwardRenderer`/`DeferredRenderer::enableCsmShadows()`
  + `setShadowZRange()`. New `Program::setUniform` overloads (`float[]`, `Mat4[]`) and
  loader entry `glUniform1fv`. Tests: `test_shadow.cpp` (unit) + `gl_smoke_csm.cpp`
  (gl_context); tutorial 33 (en + de).

### Added
- F31 — `render::RenderGraph` (`render/graph.hpp`): declarative render-pass graph — named
  texture resources (owned/external) + passes with input/output lists and callbacks;
  `compile()` topologically orders the passes (cycle-safe, ping-pong aware, `feedback`
  passes for temporal accumulation) and creates one FBO per pass; `execute()`,
  `setPassEnabled()`, `resize()`. The `DeferredRenderer` post chain (SSAO → lighting →
  SSR → TAA → bloom → god rays → composite) now runs through a graph (`postGraph()`).
  Math: `math::halton()`, `math::jitterViewProj()`, `SceneView::jitter`.
- F32 — TAA in `DeferredRenderer` (`taaEnabled`, `taaBlend`): Halton subpixel jitter on
  the G-buffer pass + history accumulation with 3x3 neighborhood clamp.
- F33 — SSR in `DeferredRenderer` (`ssrEnabled`, `ssrStrength`, `ssrMaxSteps`,
  `ssrThickness`): world-space raymarch against the G-buffer position texture.

### Added
- F34 — GPU culling + indirect draw: `render::GpuCuller` (`render/culling.hpp`) — compute
  shader (GL 4.3) frustum-culls instances via world bounding spheres
  (`math::transformSphere` + `math::Frustum`), compacts visible model matrices into an
  SSBO that doubles as the divisor-1 instance attribute (locations 4..7), writes the
  visible count into the indirect command (`atomicMax`), and `draw()` issues a single
  `glDrawElementsIndirect`/`glDrawArraysIndirect`. `Mesh` gained `boundingSphere()` +
  `vertexBuffer()/indexBuffer()`; new `Buffer::Target::DrawIndirect` + barrier-bit
  constants. Tests: `test_culling.cpp` (unit), `gl_smoke_culling.cpp` (gl_context, skips
  < GL 4.3); tutorial 37 (en + de).
- F35 — bindless textures: `gl/ext/bindless.hpp` — `GL_ARB_bindless_texture` entry points
  via `gl::procAddress`, RAII `ext::BindlessTexture`, `ext::setUniformHandle()`,
  `ext::supported()`; guarded (macOS without the extension reports false). Test:
  `gl_smoke_bindless.cpp` (gl_context, skips 77 without the extension); tutorial 38
  (en + de).
- Tests: `test_graph.cpp` (unit), `gl_smoke_graph.cpp` / `gl_smoke_taa.cpp` /
  `gl_smoke_ssr.cpp` (gl_context); tutorials 34–36 (en + de).

### Added
- F36 — depth of field (bokeh) in `DeferredRenderer`: `dofEnabled`, `dofFocusDist`,
  `dofFocusRange`, `dofStrength`, `dofTaps`. New `dof` pass in the post graph (after TAA)
  computes the circle of confusion from the G-buffer position via the pure, unit-tested
  `math::cocRadius()` and gathers on a golden-angle spiral weighted by
  `max(tapCoC, ownCoC)`; the composite reads `dof`. Tests: `test_dof.cpp` (unit),
  `gl_smoke_dof.cpp` (gl_context); screenshot `examples/features --scene dof`; tutorial 39
  (en + de).
- F37 — volumetric fog / light shafts in `DeferredRenderer`: `volFogEnabled`,
  `volFogDensity`, `volFogColor`, `volFogAnisotropy`, `volFogHeight`/`volFogFalloff`,
  `volFogShadows`, `volFogSteps`, `volFogMaxDist`. New `volfog` pass in the post graph
  (after DoF) raymarches the world-space view ray per pixel (G-buffer depth for geometry,
  far plane for sky) and accumulates single scattering from the sun with the pure,
  unit-tested `math::henyeyGreenstein()` phase function; optional exponential height fog;
  optional shadow-map/CSM attenuation of the in-scattering (reuses `kShadowUniforms` +
  `kShadowFunc` from `lighting_glsl.hpp`, texture units 4/10 with 1x1 dummies when off).
  The composite reads `volfog`; disabled = copy. Tests: `test_volfog.cpp` (unit),
  `gl_smoke_volfog.cpp` (gl_context); screenshot `examples/features --scene volfog`;
  tutorial 40 (en + de).
- F38 — heightmap terrain (`render/terrain.hpp`): `render::HeightmapTerrain` builds a
  landscape mesh from a heightmap (float grid or grayscale `procedural::Image`) — grid
  mesh with finite-difference normals, height/slope vertex coloring (low→high, steep→rock),
  texturing UVs, +Y CCW winding; `heightAt(x, z)` gives bilinear world-space heights for
  placing objects. Free helpers: `heightmapSample()` (float grid + Image),
  `heightmapTerrainData()` (raw `MeshData`), and `procedural::heightmap(w, h, seed, ...)`
  as a deterministic seeded fBm heightmap source. Tests: `test_terrain.cpp` (unit),
  `gl_smoke_terrain.cpp` (gl_context); screenshot `examples/features --scene heightmap`;
  tutorial 41 (en + de).
- F39 — backward compatibility via capability detection: `render::effectSupported()`
  (render/effects.hpp) is a pure, unit-tested mapping from (GL version, bindless, compute)
  to feature support; `gl::Features` gained `textureArrays` + `bindlessTextures`. Both
  renderers capture `gl::queryDeviceInfo()` at `create()` and clear unsupported effect
  flags in `render()` (`applyCapabilities()`, one-time `warnUnsupported()` warning; the IBL
  environment is dropped the same way) — features are simply not used when the GPU cannot
  run them. Tests: `test_effects.cpp` (unit), `gl_smoke_effects.cpp` (gl_context).
- F40 — uniform effect toggle API on `DeferredRenderer` AND `ForwardRenderer`:
  `render::Effect` enum + `setEffectEnabled()` / `effectEnabled()` (requested AND
  supported) / `setAllEffects()` / `effectSupported()`; the existing public fields stay
  valid and are routed through the same gating; `DeferredRenderer` gained `bloomEnabled`.
  Tutorial 42 (en + de).
- F41 — level of detail: `render::LodMesh` (render/lod.hpp) holds the detail levels +
  ascending switch distances (`selectLevel`/`select`, `defaultDistances()` screen-space
  heuristic, `setDistances()`); all pipelines resolve `DrawItem::lod` per item by camera
  distance (`resolveLodMesh()` in the deferred G-buffer, forward and both shadow passes);
  both renderers expose `lodEnabled` (also `Effect::Lod`, always supported — CPU-side) and
  `lastLodLevel()`; `terrainLods()` builds a multi-resolution terrain LodMesh from a
  heightmap. Tests: `test_lod.cpp` (unit), `gl_smoke_lod.cpp` (gl_context); screenshot
  `examples/features --scene lod`; tutorial 43 (en + de).
- F42 — post-processing for the forward renderer: new `render::PostChain`
  (render/post_chain.hpp) runs a RenderGraph over any HDR input (TAA → DoF → volumetric
  fog → bloom → god rays → ACES composite into an LDR target); DoF/vol fog reconstruct
  world positions from the depth texture, so no G-buffer is needed (SSAO/SSR remain
  deferred-only). `ForwardRenderer::enablePostFX()` switches to an HDR forward pass +
  post chain (off by default — the old direct-LDR path is unchanged), `postChain()` exposes
  the config, post effects are part of the F40 `Effect` toggle API and F39-gated.
  Shared post shaders moved to `src/render/postfx_glsl.hpp` (used by both chains).
  `examples/forward --fx` renders a post-FX screenshot. Test: `gl_smoke_forward_fx.cpp`
  (gl_context). Tutorial 44 (en + de).

### Changed
- All tutorial screenshots re-rendered with restrained, aesthetic effect settings: lower
  bloom/god-ray weights, subtle DoF (strength 9/6/7 instead of 18/10/10), gentler
  volumetric fog densities, softer showcase exposure (1.0). Scenes themselves unchanged.
- Showcase reworked into an organic landscape: noise heightfield terrain with a clearing
  and a pond basin, 40 CSG trees (pine + broadleaf), 22 bushes, 14 rocks, the monolith,
  3000 instanced grass blades (`Mesh::setInstanceData`, F24) and a metallic pond slab
  reflecting the sky via SSR; the renderer now also runs F36 DoF (focus on the monolith)
  and F37 volumetric fog (mode-dependent, shadowed shafts on day/sunset).
  `readme-showcase-{day,sunset,night}.png` re-rendered by the CI `docs` job.
- Screenshots reworked: new `examples/features` renders one deterministic headless PNG
  per feature scene (`--scene noise|pbr|ibl|csm|taa|ssr|culling|bindless|dof|volfog`) for
  tutorials 30–40; the CI `docs` job renders them plus a new `readme-showcase-night.png`.
  `examples/showcase` gained `--mode night` (dark sky, IBL environment baked from a
  gradient cubemap, emissive lamp posts reflected via SSR) and now uses CSM shadows, TAA
  and SSR in all modes (headless renders 8 frames so the TAA history accumulates).
  Tutorials 12, 29–38 (en + de) and both READMEs reference the new images.

### Added
- F43 — renderer demo: `render::DemoScene` (`render/demo.hpp`, PIMPL in
  `src/render/demo.cpp`) owns the whole showcase landscape (noise heightfield terrain +
  pond, CSG trees/bushes/rocks/monolith, procedural voronoi/noise textures, 3000 instanced
  grass blades, PBR water, sun + point lights, emissive night lamps, baked IBL environment)
  and runs it through BOTH ready-to-use renderers — `configure(ForwardRenderer&)` (CSM +
  HDR post chain F42: bloom/god rays/TAA/DoF/vol fog) and `configure(DeferredRenderer&)`
  (CSM + SSAO + HDR + bloom/god rays/TAA/SSR/DoF/vol fog + gradient sky); `fillScene()`
  provides the per-frame `SceneView`, `DemoMode` day/sunset/night, `demoModeFromName()`.
  New `examples/demo` (`--renderer forward|deferred --mode ... --screenshot ...`);
  `examples/showcase` is now a thin deferred-only wrapper over `DemoScene` (identical CI
  screenshots). Test `gl_smoke_demo.cpp` (gl_context: both renderers render the demo scene
  lit, wiring + night IBL + no GL errors). CI docs job renders
  `tutorial-renderer-demo-{deferred,forward}.png`. Tutorial 45 (en + de) maps every single
  tutorial onto the demo; README.md / README.de.md updated.

## [0.1.0] — initial release

Zero-dependency C++17 OpenGL wrapper + built-in render layer. Highlights:

- Runtime GL loader (`fuhgl::gl::init()`, curated core table GL 1.0–4.6 by name,
  `gl::procAddress()` for anything else, optional GL 4.5 DSA table `gl::dsa()`),
  error handling (`gl::lastError()`, debug output/callbacks, `FUHGL_ASSERT`).
- RAII GL wrappers (PIMPL, movable): `Shader`, `Program` (+ reflection: active
  uniforms/attributes/uniform blocks), `Buffer`, `VertexArray`, `Texture`
  (2D/3D/array/cube/MSAA/buffer), `Sampler`, `Framebuffer`, `Renderbuffer`,
  `UniformBuffer`, `Query`, `Fence`; plus `ProgramPipeline` / `TransformFeedback`.
- Native window + GL context per platform (Win32/WGL, X11/GLX via `dlopen`,
  macOS Cocoa; experimental EGL/Wayland/headless backend as a build option),
  header-only math module, camera controllers (first-person / free / isometric).
- Render layer (`fuhgl/render.hpp`): forward + deferred pipelines and ready-to-use
  `ForwardRenderer` / `DeferredRenderer` with lights (directional/point/spot),
  PCF shadow mapping, SSAO, HDR + bloom + crepuscular god rays, parallax/bump
  mapping, fog, ACES tone mapping; procedural textures & preset materials; CSG
  solids with UV-mapped texturing; instancing; wireframe overlay; bitmap-text/FPS
  overlay. 29 tutorials with CI-rendered screenshots.
- Tests: CPU unit tests (label `unit`) + GL-context smoke tests (label `gl_context`,
  reported as *Skipped* via exit code 77 when no context is available).

[0.1.0]: https://github.com/FuhMhanShu/FuhGLCore/releases/tag/v0.1.0
