# FuhGLCore

**A zero-dependency C++17 OpenGL toolkit — from the GL core up to a ready-to-use
render layer.** Only the C++ standard library and platform system APIs are used:
no GLAD/GLEW, no GLFW/SDL, no Khronos headers, no vcpkg.

Bottom layer: a runtime OpenGL loader (GL 1.0–4.6 resolved by name at runtime),
thin RAII wrappers, native windows + GL contexts (Win32/WGL, X11/GLX, macOS Cocoa,
plus an experimental EGL backend on Linux with Wayland and surfaceless headless),
a header-only math module and camera controllers. On top of that sits the built-in
render layer (`fuhgl/render.hpp`) — forward & deferred pipelines with lights, PCF
shadow mapping, SSAO, HDR post-processing (bloom, god rays, ACES tone mapping),
procedural textures & materials, CSG solids, text/FPS overlay, wireframe overlay
and instancing.

> 🇩🇪 **Deutsche Version:** [README.de.md](README.de.md) · Deutsche Doku: [docs/de/](docs/de/tutorials/README.md)

The name "Core" refers to the zero-dependency GL/window/math core; the built-in
render layer on top (`fuhgl/render.hpp` — pipelines, default renderers, post FX,
solids/CSG, text, wireframe, instancing) is a small module in the same library, not a
separate engine project.

![showcase day](docs/images/readme-showcase-day.png)
![showcase sunset](docs/images/readme-showcase-sunset.png)
![showcase night](docs/images/readme-showcase-night.png)

Landscape rendered with the `DeferredRenderer` (CSM shadows, SSAO, HDR + bloom, god rays,
fog, TAA, SSR; the night look adds an IBL environment and lamp reflections) — see
`examples/showcase` and [tutorial 29](docs/en/tutorials/29-showcase-scene.md).

![first triangle](docs/images/tutorial-first-triangle.png)
![isometric skybox](docs/images/tutorial-isometric-skybox.png)

*(Screenshots rendered headless in CI — see [tutorial 12](docs/en/tutorials/12-headless-rendering-and-screenshots.md))*

## What you get

- **Runtime GL loader** — `fuhgl::gl::init()` loads core functions (GL 1.0–4.6) by name at
  runtime (whatever the platform provides — macOS caps at GL 4.1, Windows/Linux go up
  to 4.6); `gl::procAddress(name)` for extensions/DSA; optional GL 4.5 DSA table (`gl::dsa()`).
- **RAII wrappers** (PIMPL, movable, non-copyable) — `Shader`, `Program`, `Buffer`,
  `VertexArray`, `Texture` (2D/3D/array/cube/multisample), `Sampler`, `Framebuffer`,
  `Renderbuffer`, `UniformBuffer`, `Query`, `Fence`.
- **Native window + GL context** — `fuhgl::Window`: Win32/WGL, X11/GLX, macOS/Cocoa,
  and on Linux an EGL backend (Wayland or surfaceless/headless). No windowing library needed.
- **Math** — `fuhgl::math`: `Vec2/3/4`, `Mat3/Mat4` (column-major, ready for
  `glUniformMatrix*fv`), `perspective/ortho/lookAt/...`, `Frustum`, picking rays,
  `Quat` (rotations + `slerp`; `toMat4(q)` matches `rotate(angle, axis)`) and seeded
  `Noise` (Perlin 2D/3D + fBm) — see [tutorial 30](docs/en/tutorials/30-math-quaternions-noise.md).
- **Cameras** — `FirstPersonCamera`, `FreeCamera` (6-DOF), `IsometricCamera`, wired to
  window events via `attach(window)`.
- **Skybox** — cubemap skybox with the far-plane depth trick, from 6 images or procedural.
- **Extras** — compute shaders, timer queries/fences, indirect draws, MSAA resolve,
  dependency-free GLSL `#include "..."`.
- **Render pipelines** — `fuhgl::render`: `ForwardPipeline` and `DeferredPipeline`
  (G-buffer + lighting pass, optional MSAA) driven by a plain `SceneView`
  (see [tutorial 13](docs/en/tutorials/13-render-pipelines.md)).
- **Device info & capabilities** — `gl::queryDeviceInfo()`: GPU vendor/renderer, GL/GLSL
  version, extension list, implementation limits and feature flags (DSA, compute, …),
  plus a printable report (see [tutorial 14](docs/en/tutorials/14-device-info-and-capabilities.md)).
- **Buffer/texture power-ups** — `Buffer::mapRange()` streaming, `gl::copyBufferSubData()`,
  compressed texture upload (`Texture::compressedImage2D()`), texture readback
  (`gl::getTexImage`), GL 4.3 vertex binding, compute image I/O (`gl::bindImageTexture`,
  `gl::dispatchComputeIndirect`) — see [tutorials 15](docs/en/tutorials/15-buffer-streaming-and-copies.md)
  and [16](docs/en/tutorials/16-textures-images-and-compute-io.md).
- **Procedural textures, preset materials, solids & CSG** — `render::procedural` generates
  textures in code (checker/noise/voronoi/…), `render::materials` provides
  matte/textured/emissive presets, and `render::Solid` + CSG operators
  (`makeCube(...) - makeSphere(...)`) mesh analytic bodies via marching tetrahedra —
  with selectable UV mapping modes (`render::SolidUv`: planar XZ/XY/YZ, spherical,
  cylindrical) so textured materials work on any solid —
  see [tutorials 17](docs/en/tutorials/17-procedural-textures-and-materials.md) and
  [18](docs/en/tutorials/18-solids-and-csg.md).
- **Full GL state & batching** — blending/stencil, polygon mode, viewport/scissor arrays,
  multi-draw/instanced-base draws, debug labels & groups (`gl/debug.hpp`), queries/sync
  helpers, internal-format queries, `glCopyImageSubData`/`glClearTexImage` —
  see [tutorial 19](docs/en/tutorials/19-state-batching-and-debug.md).
- **Lights** — `render::Light` with directional/point/spot types, analytic attenuation,
  range cutoff and spot cones, uploaded as a `LightBlock` UBO to the pipelines —
  see [tutorial 20](docs/en/tutorials/20-lights.md).
- **Shadow mapping** — `render::ShadowMap` (depth-only pass from the light's POV) +
  `lightViewProj()` helpers + PCF sampling in the pipelines via `SceneView::shadowMap` —
  see [tutorial 21](docs/en/tutorials/21-shadows.md).
- **Cascaded shadow maps (CSM)** — `render::CascadeShadowMap` (depth texture array, one layer
  per cascade, fitted per camera-frustum slice) via `ForwardRenderer::enableCsmShadows()` /
  `SceneView::cascades` — see [tutorial 33](docs/en/tutorials/33-cascaded-shadow-maps.md).
- **Render graph + TAA + SSR** — the deferred post chain runs through `render::RenderGraph`
  (declarative passes, auto-ordered, per-pass FBOs); optional temporal AA (Halton jitter +
  history accumulation, `taaEnabled`) and screen-space reflections (`ssrEnabled`) —
  see [tutorials 34–36](docs/en/tutorials/34-render-graph.md).
- **GPU culling + indirect draw** — `render::GpuCuller` culls instances on the GPU
  (compute shader + bounding spheres) and draws the visible ones with one indirect
  instanced draw; **bindless textures** (`GL_ARB_bindless_texture`) via
  `gl::ext::BindlessTexture` — see [tutorials 37–38](docs/en/tutorials/37-gpu-culling-indirect.md).
- **Depth of field (bokeh)** — `DeferredRenderer::dofEnabled` with a focus plane and
  circle-of-confusion gather on the HDR image — see [tutorial 39](docs/en/tutorials/39-dof.md).
- **Volumetric fog / light shafts** — `DeferredRenderer::volFogEnabled`: raymarched single
  scattering with Henyey-Greenstein phase, height fog and shadow-mapped in-scattering —
  see [tutorial 40](docs/en/tutorials/40-volfog.md).
- **Heightmap terrain** — `render::HeightmapTerrain` builds a landscape mesh from a
  heightmap (float grid or grayscale image) with slope-based coloring; `heightAt()` places
  objects on it — see [tutorial 41](docs/en/tutorials/41-heightmap-terrain.md).
- **PBR materials** — metallic-roughness shading (Cook-Torrance GGX) in both pipelines:
  `materials::pbr(albedo, metallic, roughness, ao)`, stored per pixel in a 5th G-buffer
  attachment in the deferred path — see [tutorial 31](docs/en/tutorials/31-pbr-materials.md).
- **IBL (image-based lighting)** — `render::Environment` bakes an irradiance cubemap, a
  GGX-prefiltered env cubemap and a BRDF LUT from any cubemap (deterministic CPU bake);
  set `SceneView::env` (or `ForwardRenderer::setEnvironment()`) — see [tutorial 32](docs/en/tutorials/32-ibl-environment.md).
- **Default forward renderer** — `render::ForwardRenderer`: one `render()` call does the
  shadow depth pass + lit forward pass with lights, PCF shadows and distance fog —
  see [tutorial 22](docs/en/tutorials/22-forward-renderer.md).
- **Default deferred renderer + post FX** — `render::DeferredRenderer`: G-buffer with
  parallax/bump mapping, SSAO, HDR lighting with shadows, bloom, crepuscular god rays
  and ACES tone mapping — see [tutorial 23](docs/en/tutorials/23-deferred-renderer-postfx.md).
- **Text & FPS overlay** — `render::TextRenderer` with an embedded public-domain 8x8 bitmap
  font (one batched draw call per frame) plus `render::FpsCounter` — see
  [tutorial 24](docs/en/tutorials/24-text-and-fps-overlay.md).
- **Wireframe overlay** — `render::Wireframe`: solids (triangle edges) and bounding boxes
  (AABB edges) as one batched `GL_LINES` draw — see
  [tutorial 25](docs/en/tutorials/25-wireframe-solids-and-bounding-boxes.md).
- **Instancing** — `Mesh::setInstanceData()` puts per-instance model matrices on the GPU
  (divisor-1 `mat4` attribute); one draw call renders hundreds of copies, animated via
  `updateInstanceData()` — see [tutorial 28](docs/en/tutorials/28-instancing.md).

Baseline: **OpenGL 3.3 core** (higher versions up to 4.6 loaded opportunistically where
the platform provides them; macOS caps at 4.1). The API never throws for domain errors —
failures return `bool`/info-log strings (the C++ standard library itself may still throw,
e.g. `std::bad_alloc`); debug builds assert and forward GL debug output to a callback.

## Quick start

```cpp
#include <fuhgl/fuhgl.hpp>

int main() {
    fuhgl::Window window;
    if (!window.create("Hello", 800, 600)) return 1;  // native window + 3.3 core context + gl::init()

    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    vs.compile(vertexSrc); fs.compile(fragmentSrc);
    fuhgl::Program prog; prog.create(); prog.attach(vs); prog.attach(fs); prog.link();

    fuhgl::FirstPersonCamera cam; cam.attach(window);
    while (!window.shouldClose()) {
        window.pollEvents();
        cam.update(1.f / 60.f);
        fuhgl::gl::clearColor(0.1f, 0.1f, 0.12f, 1.f);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);
        prog.use();
        prog.setUniform("uViewProj", cam.viewProj(800.f / 600.f));
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
        window.swapBuffers();
    }
}
```

Full walkthrough: [tutorial 01 — window and first triangle](docs/en/tutorials/01-window-and-first-triangle.md).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix <prefix>   # provides find_package(FuhGLCore)
ctest --test-dir build --output-on-failure
```

Options: `-DFUHGL_BUILD_EXAMPLES=ON` (default), `-DFUHGL_BUILD_TESTS=ON` (default),
`-DFUHGL_LINUX_BACKEND=X11|EGL` (default X11; EGL adds Wayland + surfaceless headless),
`-DFUHGL_ENABLE_HARDENING=ON` (default; compiler/linker hardening flags, see
[DEVELOPMENT.md](docs/DEVELOPMENT.md)).

Only a C++17 compiler and CMake ≥ 3.20 are required — GL functions are loaded at runtime,
so no OpenGL dev packages are needed at build time.

## Documentation

- [Documentation overview / Sprachauswahl](docs/README.md)
- [User guide](docs/USER_GUIDE.md) — concepts and features (the descriptive part, English)
- [Tutorials (English)](docs/en/tutorials/README.md) — one small tutorial per feature, with screenshots;
  [Tutorials (Deutsch)](docs/de/tutorials/README.md) — German translation
- [Development](docs/DEVELOPMENT.md) — architecture, conventions, build, tests, CI (the technical part, English)
- [Requirements](docs/REQUIREMENTS.md) — the agreed functional/non-functional requirements (English)
- API reference — Doxygen HTML is built in CI and uploaded as the `doxygen-html` artifact
  (locally: `cd docs && doxygen Doxyfile`)

## Security

Found a vulnerability? Please report it privately (no public issues) —
see [SECURITY.md](SECURITY.md).

## License

MIT — see [LICENSE](LICENSE).
