# 23 — Default deferred renderer + post-processing (F17)

**Goal:** `render::DeferredRenderer` — a ready-to-use deferred renderer with the full
post chain: **SSAO**, **HDR** lighting, **bloom**, **crepuscular (god) rays**, ACES tone
mapping, plus **parallax occlusion** and **bump/normal mapping** in the G-buffer pass
and **PCF shadows** from tutorial 21.

![deferred + post FX](images/tutorial-deferred-fx.png)
*Full chain (SSAO + shadows + bloom + god rays + parallax floor). Compare `--no-fx`:
`tutorial-deferred-fx-nofx.png`.*

## Usage

```cpp
fuhgl::render::DeferredRenderer renderer;
renderer.create(800, 600);            // G-buffer + post chain; optional MSAA: create(w, h, 4)
renderer.enableShadows(1024);
renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 8.f);

// knobs (all optional, sane defaults):
renderer.ssaoEnabled = true;  renderer.ssaoRadius = 0.6f;  renderer.ssaoStrength = 1.2f;
renderer.bloomStrength = 0.8f; renderer.bloomThreshold = 1.0f; renderer.bloomBlurPasses = 2;
renderer.godRaysEnabled = true; renderer.rayDensity = 0.92f; renderer.rayDecay = 0.95f; renderer.rayWeight = 0.4f;
renderer.exposure = 1.1f;

fuhgl::render::SceneView scene;
scene.camera = &cam; scene.width = 800; scene.height = 600; scene.aspect = 800.f/600.f;
scene.lights.push_back(fuhgl::render::Light::directional({0.35f, -0.85f, 0.5f}, {1.f, 0.88f, 0.7f}, 1.8f));
scene.items.push_back({&floorMesh, &parallaxFloorMat, fuhgl::math::translate({0, -0.25f, 0})});
...

renderer.render(scene);               // everything in one call
// result is LDR RGBA8 in renderer.output() (blit to the window with Framebuffer::blitToDefault)
```

## The frame, pass by pass

1. **Shadow depth pass** (if enabled): `ShadowMap::render` from the first directional/
   spot light (`lightViewProj`).
2. **G-buffer**: `DeferredPipeline::renderGBuffer()` — albedo / normal / world position
   (RGBA16F) / emissive MRT (+ MSAA resolve). Supports **parallax occlusion mapping**
   (height map, unit 5) and **bump mapping** (normal map, unit 4): the fragment shader
   builds a tangent frame from screen-space derivatives, ray-marches the height layers
   along the view ray and perturbs the normal — no tangent vertex attribute needed.
3. **SSAO**: hemisphere kernel (deterministic LCG, injected into the shader) around the
   world-space normal, projected with the camera VP, depth-compared against the
   position buffer; then a separable 9-tap blur. Sampled in the lighting pass (unit 5).
4. **HDR lighting**: `renderLighting()` into an RGBA16F target — lights (tutorial 20),
   PCF shadows (tutorial 21), fog, SSAO multiply, sky gradient + **HDR sun disk**
   (`SceneView::sunDirection/sunColor/sunCos`, derived from the first directional light).
5. **Bloom**: bright-pass (luminance threshold) + separable gaussian blur ping-pong.
6. **Crepuscular rays**: radial blur of the HDR buffer toward the sun's screen position
   (`render::sunScreenUV(camera, sunDir, w, h)`), decay-weighted 48 samples.
7. **Composite**: `scene·exposure + bloom·strength + rays` → **ACES** filmic tone map →
   gamma 2.2 → LDR RGBA8 `output()`.

## Parallax / bump materials

```cpp
auto heightImg = fuhgl::render::procedural::noise(256, 256, 42u, 5);       // height (uses .r)
fuhgl::Texture heightTex = fuhgl::render::procedural::makeTexture(heightImg);
fuhgl::Texture normalTex = fuhgl::render::procedural::makeNormalMap(heightImg, 3.f); // derived tangent-space normal map
fuhgl::Texture bricksTex = fuhgl::render::procedural::makeTexture(fuhgl::render::procedural::bricks(256, 256, 8, 12, brick, mortar));

fuhgl::render::Material floor = fuhgl::render::materials::parallax(bricksTex, heightTex, /*scale*/ 0.05f, /*layers*/ 20);
floor.normalMap = &normalTex;                        // bump on top
// or: fuhgl::render::materials::normalMapped(bricksTex, normalTex);
```

## Tests & example

`tests/gl_smoke_deferred_fx.cpp` toggles each effect (SSAO / bloom / god rays /
parallax) and asserts the expected pixels change; `tests/test_shadow.cpp` unit-tests
`sunScreenUV`. `examples/postfx/main.cpp` is the demo (`--screenshot`, `--no-fx`,
`--no-shadows`).
