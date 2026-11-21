# Changelog

All notable changes to FuhGLCore are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/) (the CMake project version is
`0.1.0` with same-major compatibility).

## [Unreleased]

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
