# FuhGLCore Tutorials (Deutsch)

> 🇬🇧 **English version:** [docs/en/tutorials/README.md](../../en/tutorials/README.md)

Ein kleines Tutorial pro Feature. Jedes ist in sich abgeschlossen: Ziel → Code → Ergebnis.
Screenshots in `docs/images/` werden headless von der CI (`docs`-Job) gerendert
(siehe [12](12-headless-rendering-and-screenshots.md)).

| # | Thema | Datei |
|---|---|---|
| 01 | Fenster + GL-Kontext + erstes Dreieck | [01-window-and-first-triangle.md](01-window-and-first-triangle.md) |
| 02 | Shader, Programme, GLSL-`#include` | [02-shaders-programs-includes.md](02-shaders-programs-includes.md) |
| 03 | Puffer und Vertex-Arrays | [03-buffers-and-vao.md](03-buffers-and-vao.md) |
| 04 | Texturen (2D/3D/Array/Cube) und Sampler | [04-textures-and-samplers.md](04-textures-and-samplers.md) |
| 05 | Framebuffer, Renderbuffer, MSAA-Resolve | [05-framebuffers-and-msaa.md](05-framebuffers-and-msaa.md) |
| 06 | Skybox | [06-skybox.md](06-skybox.md) |
| 07 | Kameras, Frustum-Culling, Picking | [07-cameras-frustum-picking.md](07-cameras-frustum-picking.md) |
| 08 | Uniform-Puffer-Objekte | [08-uniform-buffers.md](08-uniform-buffers.md) |
| 09 | Compute-Shader | [09-compute-shaders.md](09-compute-shaders.md) |
| 10 | Timer-Queries und Fence-Syncs | [10-queries-and-sync.md](10-queries-and-sync.md) |
| 11 | GL-4.5-DSA-Fast-Path (komplette 72-Einträge-Tabelle) | [11-dsa.md](11-dsa.md) |
| 12 | Headless-Rendering und Screenshots (EGL surfaceless) | [12-headless-rendering-and-screenshots.md](12-headless-rendering-and-screenshots.md) |
| 13 | Render-Pipelines: Forward & Deferred | [13-render-pipelines.md](13-render-pipelines.md) |
| 14 | Geräte-Infos & Fähigkeiten (GPU, Erweiterungen, Limits) | [14-device-info-and-capabilities.md](14-device-info-and-capabilities.md) |
| 15 | Puffer-Streaming, GPU-Kopien, GL-4.3-Vertex-Binding | [15-buffer-streaming-and-copies.md](15-buffer-streaming-and-copies.md) |
| 16 | Textur-Upload/Readback, komprimierte Texturen, Compute-Image-I/O | [16-textures-images-and-compute-io.md](16-textures-images-and-compute-io.md) |
| 17 | Prozedurale Texturen & vordefinierte Materialien | [17-procedural-textures-and-materials.md](17-procedural-textures-and-materials.md) |
| 18 | Körper & CSG (Würfel/Kugel/Zylinder/Torus, Vereinigung/Differenz/Schnitt) + texturierte Körper (UV-Modi: planar/sphärisch/zylindrisch) | [18-solids-and-csg.md](18-solids-and-csg.md) |
| 19 | GL-State, Draw-Batching, Debug-Labels & Image-Ops | [19-state-batching-and-debug.md](19-state-batching-and-debug.md) |
| 20 | Lichtquellen: direktional, Punkt & Spot | [20-lights.md](20-lights.md) |
| 21 | Shadow Mapping (Depth-Pass + PCF) | [21-shadows.md](21-shadows.md) |
| 22 | Default-Forward-Renderer (Licht + Schatten + Nebel) | [22-forward-renderer.md](22-forward-renderer.md) |
| 23 | Default-Deferred-Renderer + Post-FX (SSAO, HDR, Bloom, God Rays, Parallax/Bump) | [23-deferred-renderer-postfx.md](23-deferred-renderer-postfx.md) |
| 24 | Text & FPS-Overlay (eingebettete 8x8-Bitmap-Font-HUD) | [24-text-and-fps-overlay.md](24-text-and-fps-overlay.md) |
| 25 | Wireframe: Körper & Bounding Boxes als Liniengitter | [25-wireframe-solids-and-bounding-boxes.md](25-wireframe-solids-and-bounding-boxes.md) |
| 26 | Programm-Pipelines, Transform Feedback, Shader-Binaries, Subroutinen, GL 4.6 (P3) | [26-program-pipelines-transform-feedback-binaries.md](26-program-pipelines-transform-feedback-binaries.md) |
| 27 | Programm-Reflexion (aktive Uniforms, Attribute, Uniform-Blöcke, Ressourcen) | [27-program-reflection.md](27-program-reflection.md) |
| 28 | Instancing: Modellmatrizen pro Instanz, ein Draw-Call pro Mesh | [28-instancing.md](28-instancing.md) |
| 29 | Showcase-Szene: Landschaft mit DeferredRenderer voller FX (README-Hero-Bilder) | [29-showcase-scene.md](29-showcase-scene.md) |
| 30 | Quaternionen & geseedetes Noise (Perlin / fBm) in `fuhgl::math` | [30-math-quaternions-noise.md](30-math-quaternions-noise.md) |

Voraussetzung für alle: Bibliothek bauen und `FuhGLCore::fuhglcore` linken
(siehe [DEVELOPMENT.md](../../DEVELOPMENT.md#build-options)).
