# Changelog

All notable changes to FuhGLCore are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/) (the CMake project version is
`0.1.0` with same-major compatibility).

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
