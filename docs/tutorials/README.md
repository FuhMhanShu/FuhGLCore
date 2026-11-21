# FuhGLCore Tutorials

One small tutorial per feature. Each is self-contained: goal → code → result.
Screenshots in `docs/images/` are rendered headless by the CI `docs` job
(see [12](12-headless-rendering-and-screenshots.md)).

| # | Topic | File |
|---|---|---|
| 01 | Window + GL context + first triangle | [01-window-and-first-triangle.md](01-window-and-first-triangle.md) |
| 02 | Shaders, programs, GLSL `#include` | [02-shaders-programs-includes.md](02-shaders-programs-includes.md) |
| 03 | Buffers and vertex arrays | [03-buffers-and-vao.md](03-buffers-and-vao.md) |
| 04 | Textures (2D/3D/array/cube) and samplers | [04-textures-and-samplers.md](04-textures-and-samplers.md) |
| 05 | Framebuffers, renderbuffers, MSAA resolve | [05-framebuffers-and-msaa.md](05-framebuffers-and-msaa.md) |
| 06 | Skybox | [06-skybox.md](06-skybox.md) |
| 07 | Cameras, frustum culling, picking | [07-cameras-frustum-picking.md](07-cameras-frustum-picking.md) |
| 08 | Uniform buffer objects | [08-uniform-buffers.md](08-uniform-buffers.md) |
| 09 | Compute shaders | [09-compute-shaders.md](09-compute-shaders.md) |
| 10 | Timer queries and fence syncs | [10-queries-and-sync.md](10-queries-and-sync.md) |
| 11 | GL 4.5 DSA fast path (full 72-entry table) | [11-dsa.md](11-dsa.md) |
| 12 | Headless rendering and screenshots (EGL surfaceless) | [12-headless-rendering-and-screenshots.md](12-headless-rendering-and-screenshots.md) |
| 13 | Render pipelines: forward & deferred | [13-render-pipelines.md](13-render-pipelines.md) |
| 14 | Device info & capabilities (GPU, extensions, limits) | [14-device-info-and-capabilities.md](14-device-info-and-capabilities.md) |
| 15 | Buffer streaming, GPU copies, GL 4.3 vertex binding | [15-buffer-streaming-and-copies.md](15-buffer-streaming-and-copies.md) |
| 16 | Texture upload/readback, compressed textures, compute image I/O | [16-textures-images-and-compute-io.md](16-textures-images-and-compute-io.md) |
| 17 | Procedural textures & predefined materials | [17-procedural-textures-and-materials.md](17-procedural-textures-and-materials.md) |
| 18 | Solids & CSG (cube/sphere/cylinder/torus, union/difference/intersection) | [18-solids-and-csg.md](18-solids-and-csg.md) |
| 19 | GL state, draw batching, debug labels & image ops | [19-state-batching-and-debug.md](19-state-batching-and-debug.md) |
| 20 | Lights: directional, point & spot | [20-lights.md](20-lights.md) |
| 21 | Shadow mapping (depth pass + PCF) | [21-shadows.md](21-shadows.md) |
| 22 | Default forward renderer (lights + shadows + fog) | [22-forward-renderer.md](22-forward-renderer.md) |
| 23 | Default deferred renderer + post FX (SSAO, HDR, bloom, god rays, parallax/bump) | [23-deferred-renderer-postfx.md](23-deferred-renderer-postfx.md) |
| 24 | Text & FPS overlay (embedded 8x8 bitmap font HUD) | [24-text-and-fps-overlay.md](24-text-and-fps-overlay.md) |
| 25 | Wireframe: solids & bounding boxes as line grids | [25-wireframe-solids-and-bounding-boxes.md](25-wireframe-solids-and-bounding-boxes.md) |
| 26 | Program pipelines, transform feedback, shader binaries, subroutines, GL 4.6 (P3) | [26-program-pipelines-transform-feedback-binaries.md](26-program-pipelines-transform-feedback-binaries.md) |
| 27 | Program reflection (active uniforms, attributes, uniform blocks, resources) | [27-program-reflection.md](27-program-reflection.md) |

Prerequisite for all: build the library and link `FuhGLCore::fuhglcore`
(see [DEVELOPMENT.md](../DEVELOPMENT.md#build-options)).
