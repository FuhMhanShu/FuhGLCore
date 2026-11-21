# 14 — Device info & capabilities

**Goal:** read out what the current OpenGL context / GPU actually offers —
vendor & renderer strings, GL/GLSL version, extension list, implementation
limits and feature flags — via one call: `fuhgl::gl::queryDeviceInfo()`.

![capability bars](../images/tutorial-gpuinfo.png)

*(Screenshot rendered headless in CI: `examples/gpuinfo --screenshot` draws the
key limits as bars — see [tutorial 12](12-headless-rendering-and-screenshots.md).)*

## Query everything at once

```cpp
fuhgl::Window window;
window.create("app", 800, 600);          // creates a context and calls gl::init()

fuhgl::gl::DeviceInfo info = fuhgl::gl::queryDeviceInfo();
std::printf("%s", info.report().c_str()); // human-readable dump
```

`DeviceInfo` is a plain value type (no PIMPL — it holds only strings/ints):

| Field | Meaning |
|---|---|
| `vendor`, `renderer`, `versionString`, `glslVersion` | raw `glGetString` values |
| `version` | parsed `gl::Version` (`atLeast(4,5)`, …); numeric query with string fallback |
| `extensions` | full list (GL 4.3 `glGetStringi` path with legacy `GL_EXTENSIONS` fallback) |
| `limits` | curated `Limits`: max texture sizes (2D/3D/cube/array layers), renderbuffer & viewport size, vertex attribs, uniform vectors, texture units, MRT (`maxDrawBuffers`/`maxColorAttachments`), MSAA `maxSamples`, UBO/SSBO bindings **and** sizes (64-bit), compute work-group count/size/invocations, element counts, anisotropy |
| `features` | derived flags: `coreProfile`, `dsa` (= `gl::hasDsa()`), `compute`, `debugOutput`, `anisotropicFiltering`, `shaderStorage` |

## Individual pieces

```cpp
info.hasExtension("GL_EXT_texture_filter_anisotropic"); // exact match in the list
fuhgl::gl::hasExtension("GL_KHR_debug");                 // context-wide helper
fuhgl::gl::extensions();                                 // std::vector<std::string>

if (info.features.dsa)      // use gl::dsa() fast path (tutorial 11)
if (info.features.compute)  // gl::dispatchCompute is safe (tutorial 09)
if (info.limits.maxSamples >= 4)
    pipeline.create(w, h, /*msaaSamples=*/4);            // MSAA G-buffer (tutorial 13)
```

Without a current context `queryDeviceInfo()` returns an empty `DeviceInfo`
(version `{0,0}`) — it never throws, like the rest of the library.

## Example

`examples/gpuinfo` prints the full report and renders the limits as bars
(windowed, Esc quits):

```
./build/examples/gpuinfo
./build/examples/gpuinfo --screenshot gpuinfo.png   # headless PNG (CI docs job)
```

Tests: `tests/test_info.cpp` (value semantics, `hasExtension`, `report()` — no GL
needed) and the device-info assertions inside `tests/gl_smoke.cpp` (run headless
against a real context in CI).
