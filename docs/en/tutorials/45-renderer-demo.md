# 45 — Renderer demo: one scene, both renderers

**Goal:** run the render layer *as a demo* — one ready-made scene that works with **both**
ready-to-use renderers. `render::DemoScene` (`render/demo.hpp`) builds the showcase
landscape once (terrain, CSG props, instanced grass, lights, three looks); `configure()`
adapts the matching effect stack to a `ForwardRenderer` or `DeferredRenderer`, and
`fillScene()` provides the per-frame `SceneView`. `examples/demo` runs the same scene
through either renderer:

```
./build/examples/demo --renderer deferred --mode day     # DeferredRenderer, full FX stack
./build/examples/demo --renderer forward  --mode sunset  # ForwardRenderer, HDR post chain
```

![Demo scene, deferred renderer](../../images/tutorial-renderer-demo-deferred.png)
![Demo scene, forward renderer with post chain](../../images/tutorial-renderer-demo-forward.png)

This tutorial is the summary part: it maps every single tutorial onto the demo, so you can
see where each feature ends up in one running program.

## Usage

```cpp
fuhgl::render::DemoScene demo;
demo.create(fuhgl::render::DemoMode::Day);   // Day | Sunset | Night (needs a GL context)

fuhgl::render::DeferredRenderer renderer;    // ... or fuhgl::render::ForwardRenderer
renderer.create(w, h);
demo.configure(renderer);                    // CSM shadows + matching post FX + lights/fog/sky

fuhgl::FirstPersonCamera cam;
demo.applyStartPose(cam);

fuhgl::render::SceneView scene;
scene.camera = &cam;
scene.width = w; scene.height = h; scene.aspect = float(w) / h;
demo.fillScene(scene);                       // items + lights + fog + clear color (+ IBL at night)

renderer.render(scene);                      // deferred; forward: renderer.render(cam, scene.items, w, h)
```

Headless screenshot (CI uses this for the two images above):

```
FUHGL_BACKEND=surfaceless ./build-egl/examples/demo --screenshot tutorial-renderer-demo-deferred.png --renderer deferred --mode day --width 960 --height 540
FUHGL_BACKEND=surfaceless ./build-egl/examples/demo --screenshot tutorial-renderer-demo-forward.png  --renderer forward  --mode day --width 960 --height 540
```

`examples/showcase` is the deferred-only thin wrapper over the same `DemoScene` (README
hero images, tutorial 29) — no duplicated scene code anymore.

## Which tutorial lands where in the demo

| Tutorial | Feature | In the `DemoScene` demo |
|---|---|---|
| 06 | Skybox | gradient sky in the deferred lighting pass (`skyTop/skyBottom`); night mode bakes its IBL source from a gradient cubemap |
| 17 + 18 | Procedural textures, CSG solids | voronoi/noise textures on rock/stone/grass; trees, bushes, rocks, monolith as CSG-built meshes |
| 20 | Lights | directional sun (per mode) + point light; night adds emissive lamp posts |
| 21 + 33 | Shadows / CSM | `enableCsmShadows(2048, 4)` on **both** renderers via `configure()` |
| 22 + 23 | Forward / deferred renderer | the two renderers `examples/demo --renderer ...` switches between |
| 24 | Text HUD | FPS/effect line in both examples |
| 28 | Instancing | 3000 grass blades in 2 instanced draws |
| 29 | Showcase scene | this demo — moved into the library as `DemoScene` |
| 31 | PBR materials | textured terrain (`pbrTextured`), metallic-smooth pond (`pbr`) |
| 32 | IBL | night mode: `Environment::createFromCubemap` + `scene.env` |
| 35 + 36 | TAA, SSR | deferred: TAA on the HDR image, SSR on the pond; forward: TAA via PostChain (SSR is deferred-only) |
| 39 + 40 | DoF, volumetric fog | both renderers (forward reconstructs depth from the depth texture, tutorial 44) |
| 41 | Heightmap terrain | noise heightfield mesh + `heightAt()` for placement and the camera |
| 42 | Effect toggles + capability fallback | all demo effects go through the gated renderer APIs |
| 44 | Forward post chain | `configure(ForwardRenderer&)` enables `enablePostFX()` + bloom/god rays/TAA/DoF/vol fog |

## How it works (F43)

- `DemoScene` (PIMPL, `src/render/demo.cpp`) owns every GL object of the scene — meshes,
  textures, materials, the night IBL environment. `create(mode)` builds them once; the
  `SceneView` filled by `fillScene()` only holds pointers, so it must not outlive the
  `DemoScene`.
- `configure(ForwardRenderer&)` / `configure(DeferredRenderer&)` are the "renderer as demo"
  part: same scene, renderer-specific effect stack — CSM + lights + fog + ambient +
  clear color on both; deferred additionally gets SSAO/SSR and the gradient sky, forward
  gets `enablePostFX()` with the PostChain (bloom, god rays, TAA, DoF, vol fog).
- Three looks via `DemoMode`: **day** (high sun, mist), **sunset** (sun low in frame → long
  shadows + strong god rays), **night** (dark sky + baked IBL + emissive lamps reflected
  in the pond via SSR).
- Deterministic: all scattering uses seeded hashes, so headless CI screenshots are
  reproducible; TAA history accumulates over 8 headless frames.

## Notes

- Cost: the demo is the full effect stack — on weak GPUs the F39 capability gating turns
  unsupported effects off with a warning (tutorial 42).
- Test: `gl_smoke_demo.cpp` (label `gl_context`) renders the day scene through **both**
  renderers headless and verifies a lit image + wiring (CSM on, post FX on, lights applied,
  night IBL present) + no GL errors.
- `demoModeFromName("sunset")` parses `--mode` (case-insensitive).
