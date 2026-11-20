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
| 11 | GL 4.5 DSA fast path | [11-dsa.md](11-dsa.md) |
| 12 | Headless rendering and screenshots (EGL surfaceless) | [12-headless-rendering-and-screenshots.md](12-headless-rendering-and-screenshots.md) |
| 13 | Render pipelines: forward & deferred | [13-render-pipelines.md](13-render-pipelines.md) |

Prerequisite for all: build the library and link `FuhGLCore::fuhglcore`
(see [DEVELOPMENT.md](../DEVELOPMENT.md#build-options)).
