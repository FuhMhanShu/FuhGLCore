# FuhGLCore — Development (technical)

This is the *development-technical* part: architecture, conventions, build, tests, CI.
The descriptive part is the [README](../README.md) and [USER_GUIDE.md](USER_GUIDE.md);
the agreed requirements are in [REQUIREMENTS.md](REQUIREMENTS.md).

## Repository layout

```
include/fuhgl/          public API (headers)
  gl/                   GL types, loader, error, state, RAII wrappers, sampler, sync, ubo
  math/                 vec / mat / func / frustum (header-only)
  cam/                  camera controllers (header-only)
  render/               render pipelines: scene/mesh/material/forward/deferred
  window.hpp            native window API (PIMPL)
  skybox.hpp            skybox renderer
src/gl/                 loader + wrapper implementations (PIMPL targets)
src/platform/           window backends: window_x11.cpp, window_win32.cpp,
                        window_cocoa.mm, window_egl.cpp, window_unsupported.cpp
src/skybox.cpp
src/render/             mesh/material/forward/deferred pipeline implementations
examples/               triangle (windowed demo), screenshot (headless PNG renderer),
                        deferred (forward/deferred demo, headless --screenshot mode)
tests/                  unit tests (own mini runner, no framework) + gl_smoke
cmake/                  package config template
docs/                   USER_GUIDE.md, DEVELOPMENT.md, REQUIREMENTS.md, tutorials/,
                        Doxyfile, images/ (committed screenshots), doxygen/ (gitignored)
.github/workflows/ci.yml
```

## Architecture & conventions

- **C++17, zero third-party dependencies.** Only the standard library and platform
  system APIs. GL functions are resolved at runtime (`dlopen`/`wglGetProcAddress`/
  `dlsym`), so building needs no GL dev packages.
- **PIMPL by default** for classes that wrap GL/platform resources (`Shader`, `Buffer`,
  `Window`, …): public header declares `struct Impl; std::unique_ptr<Impl>`, the `.cpp`
  defines it. Math, camera and device-info types (`gl::DeviceInfo`/`Limits`/`Features`)
  are plain value types (no PIMPL — they hold only floats/strings, no GL handles).
- **No exceptions.** Fallible APIs return `bool` + info-log; `FUHGL_ASSERT` in debug.
- **Namespace `fuhgl`** (`fuhgl::math` for math, `fuhgl::gl` for GL functions/state).
- **Performance**: wrappers are thin inline pass-throughs; no state tracking, no
  allocations in the render path. The loader table (`gl::functions()`) is filled once
  in `gl::init()`; `gl::procAddress(name)` covers everything else (extensions, DSA).
- **English** comments/Doxygen throughout.

## Loader design

`gl::init()` opens the platform GL library and fills a curated `Functions` table
(~90 core entry points used by the wrappers, GL 1.0–4.6) plus a `DsaFunctions` table
(GL 4.5, all-null when unsupported — check `gl::hasDsa()`). Anything else is available
via `gl::procAddress("glAnyName")`. Windows note: functions above GL 1.1 need
`wglGetProcAddress` with a current context — `Window::create()` handles the bootstrap
(dummy context → `wglCreateContextAttribsARB` → real context → `gl::init()`).

## Window backends

One backend is compiled per platform (CMake): Win32/WGL, X11/GLX, Cocoa, and on Linux
optionally EGL (`-DFUHGL_LINUX_BACKEND=EGL`) which supports Wayland/xdg-shell windows
and surfaceless headless contexts (`FUHGL_BACKEND=surfaceless`). X11/EGL are loaded
via `dlopen` with own minimal ABI declarations (`src/platform/x11_min.hpp`), so no
X11/Wayland dev packages are needed. The hand-marshalled Wayland protocol in
`window_egl.cpp` is compile-verified only — X11 remains the Linux default.

## Build options

| Option | Values | Default |
|---|---|---|
| `FUHGL_BUILD_EXAMPLES` | ON/OFF | ON |
| `FUHGL_BUILD_TESTS` | ON/OFF | ON |
| `FUHGL_LINUX_BACKEND` | `X11` / `EGL` | `X11` |

Install/export provides `find_package(FuhGLCore)` → target `FuhGLCore::fuhglcore`.

## Tests

- `fuhgl_tests` — headless unit tests (own mini runner, no framework): math, cameras,
  frustum/picking, loader tables, shader-include resolver. No GL context needed.
- `gl_smoke` — headless GL render test: creates a surfaceless context (EGL backend),
  renders a triangle into an FBO, verifies the center pixel via `readPixels`.
  Skips cleanly (exit 0) when no GL implementation is available.

Run: `ctest --test-dir build --output-on-failure`

## CI (`.github/workflows/ci.yml`)

- `build-and-test` on ubuntu/windows/macos: build (Debug-equivalent Release), unit
  tests; on Linux additionally the EGL-backend build + `gl_smoke` under Mesa, and the
  windowed example under Xvfb.
- `docs` (ubuntu): builds **Doxygen HTML** (`cd docs && doxygen Doxyfile`) and renders
  the **tutorial screenshots** headless (`examples/screenshot`, surfaceless EGL →
  `docs/images/*.png`); both are uploaded as artifacts (`doxygen-html`,
  `tutorial-screenshots`). The committed screenshots in `docs/images/` were produced
  by this job.

## Adding a feature

1. Public API in `include/fuhgl/…` (Doxygen comments, English), PIMPL impl in `src/`.
2. Extend the curated loader table in `src/gl/loader.cpp` if you need new GL entry points.
3. Unit tests in `tests/` (headless where possible).
4. Tutorial in `docs/tutorials/` (+ screenshot via the `docs` CI job if it renders).
5. Update `README.md` / `USER_GUIDE.md` feature table.
