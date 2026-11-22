# FuhGLCore Tutorials

> 🇩🇪 **Deutsche Version:** [docs/de/tutorials/README.md](../../de/tutorials/README.md)

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
| 18 | Solids & CSG (cube/sphere/cylinder/torus, union/difference/intersection) + textured solids (UV modes: planar/spherical/cylindrical) | [18-solids-and-csg.md](18-solids-and-csg.md) |
| 19 | GL state, draw batching, debug labels & image ops | [19-state-batching-and-debug.md](19-state-batching-and-debug.md) |
| 20 | Lights: directional, point & spot | [20-lights.md](20-lights.md) |
| 21 | Shadow mapping (depth pass + PCF) | [21-shadows.md](21-shadows.md) |
| 22 | Default forward renderer (lights + shadows + fog) | [22-forward-renderer.md](22-forward-renderer.md) |
| 23 | Default deferred renderer + post FX (SSAO, HDR, bloom, god rays, parallax/bump) | [23-deferred-renderer-postfx.md](23-deferred-renderer-postfx.md) |
| 24 | Text & FPS overlay (embedded 8x8 bitmap font HUD) | [24-text-and-fps-overlay.md](24-text-and-fps-overlay.md) |
| 25 | Wireframe: solids & bounding boxes as line grids | [25-wireframe-solids-and-bounding-boxes.md](25-wireframe-solids-and-bounding-boxes.md) |
| 26 | Program pipelines, transform feedback, shader binaries, subroutines, GL 4.6 (P3) | [26-program-pipelines-transform-feedback-binaries.md](26-program-pipelines-transform-feedback-binaries.md) |
| 27 | Program reflection (active uniforms, attributes, uniform blocks, resources) | [27-program-reflection.md](27-program-reflection.md) |
| 28 | Instancing: per-instance model matrices, one draw call per mesh | [28-instancing.md](28-instancing.md) |
| 29 | Showcase scene: landscape with DeferredRenderer full FX (README hero images) | [29-showcase-scene.md](29-showcase-scene.md) |
| 30 | Quaternions & seeded noise (Perlin / fBm) in `fuhgl::math` | [30-math-quaternions-noise.md](30-math-quaternions-noise.md) |
| 31 | PBR materials: metallic-roughness (Cook-Torrance GGX) in both pipelines | [31-pbr-materials.md](31-pbr-materials.md) |
| 32 | IBL: irradiance + prefiltered env cubemap + BRDF LUT from any cubemap | [32-ibl-environment.md](32-ibl-environment.md) |
| 33 | Cascaded shadow maps (CSM): depth texture array, one layer per cascade | [33-cascaded-shadow-maps.md](33-cascaded-shadow-maps.md) |
| 34 | Render graph: declarative passes, auto-ordered, FBO per pass (DeferredRenderer post chain) | [34-render-graph.md](34-render-graph.md) |
| 35 | TAA: Halton subpixel jitter + temporal history accumulation | [35-taa.md](35-taa.md) |
| 36 | SSR: screen-space reflections on the HDR image | [36-ssr.md](36-ssr.md) |
| 37 | GPU culling + indirect draw: compute-shader frustum cull, one instanced indirect draw | [37-gpu-culling-indirect.md](37-gpu-culling-indirect.md) |
| 38 | Bindless textures (GL_ARB_bindless_texture): 64-bit handles in uint64_t uniforms | [38-bindless-textures.md](38-bindless-textures.md) |
| 39 | Depth of field (bokeh): focus plane + CoC gather pass after TAA | [39-dof.md](39-dof.md) |
| 40 | Volumetric fog / light shafts: raymarched single scattering, HG phase, height fog, shadowed in-scattering | [40-volfog.md](40-volfog.md) |
| 41 | Heightmap terrain: landscape mesh from a heightmap, slope coloring, heightAt placement | [41-heightmap-terrain.md](41-heightmap-terrain.md) |
| 42 | Effect toggles + GPU capability fallback: Effect enum, setEffectEnabled, graceful degradation | [42-effects-capabilities.md](42-effects-capabilities.md) |
| 43 | Level of detail: LodMesh levels + distance-based selection in all pipelines, toggleable | [43-lod.md](43-lod.md) |
| 44 | Forward renderer post FX: PostChain over an HDR pass (bloom, god rays, TAA, DoF, vol fog) | [44-forward-postfx.md](44-forward-postfx.md) |
| 45 | Shadow kit: depth targets (2D/array/cube, VSM moments), cube + atlas shadow maps, texel snapping, public GLSL sampling kit (PCSS/VSM/contact shadows) + DeferredRenderer integration (filter modes, cascade blend, point shadows, contact shadows) | [45-shadow-kit.md](45-shadow-kit.md) |

Prerequisite for all: build the library and link `FuhGLCore::fuhglcore`
(see [DEVELOPMENT.md](../../DEVELOPMENT.md#build-options)).
