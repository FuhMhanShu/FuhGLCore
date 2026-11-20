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

## 6. Feasibility assessment

Feasible with low-to-medium risk. Estimated MVP size ≈ 5–6k lines:
loader ~400, GL wrappers ~2k, window ~1.2k (3 platforms), math ~800,
cameras ~400, CMake/docs/tests/example ~1k. Known risks: Windows > 1.1
functions need `wglGetProcAddress` with a current context (handled by `Window`);
Wayland out of scope for v1; only Linux is compile-verifiable in the dev sandbox
(Windows/macOS verified via CI matrix). Math + cameras are fully unit-testable
headless.
