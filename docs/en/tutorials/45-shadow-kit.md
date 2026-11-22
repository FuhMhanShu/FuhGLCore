# 45 — Shadow kit: prerequisites for realistic light & shadows (F45/F46)

**Goal:** the *technical prerequisites* for realistic light and shadow casting, as reusable
building blocks — plus the `DeferredRenderer` wired on top of them. The kit itself is
deliberately **not** a renderer feature: you compose it into your own passes and shaders.

## 1. The pieces (`render/shadow_kit.hpp`)

| Piece | What it gives you |
|---|---|
| `render::ShadowTarget` | generic depth target — `Tex2D`, `Tex2DArray`, `Cube` — with an optional RGBA32F **moments** attachment (`create(kind, size, layers, moments=true)`) for VSM; `render(scene, vp, layerOrFace)` draws the scene depth (back faces, like `ShadowMap`) |
| `render::CubeShadowMap` | cube shadow map for **one point light**: `create(size, moments)`, `render(scene, pointLight, zNear, zFar)` fills all 6 faces, `faceVP(i)`, `farPlane()` |
| `render::ShadowAtlas` | **multi-light shadows**: one 2D-array layer per shadow-casting light; `renderLight(scene, layer, light, center, radius, snap)` stores the (optionally snapped) light VP per layer |
| `pointLightFaceVPs(pos, near, far)` | the 6 cube-face view-projections of a point light |
| `snapLightVPToTexels(vp, mapSize, refWorldPos)` | **cascade stabilization**: shifts the light VP so a reference point lands on a texel center — kills shadow "swimming" when the camera moves |
| `hemisphereKernel(n, seed)` / `poissonDisk(n, seed)` | deterministic sample patterns for SSAO/HBAO/contact-shadow kernels and PCF/PCSS (CPU-side, unit-testable) |

`ShadowMap` and `CascadeShadowMap` are now thin wrappers over `ShadowTarget`, so they gained
`moments` support (`create(size, moments)`, `momentsTextureId()`) and
`CascadeShadowMap::snapToTexels` — public API otherwise unchanged.

## 2. The GLSL kit (`render/shadow_glsl.hpp`)

Composable `inline constexpr` strings (GLSL 330) — assemble your own shader:

```cpp
std::string fs = std::string("#version 330 core\nout vec4 FragColor;\n")
               + fuhgl::render::kSkUniforms + fuhgl::render::kSkFuncs
               + R"GLSL(
void main() {
    vec3 p = shadowNdc(uLightVP, vWorldPos);
    float sh = shadowInBounds(p)
        ? pcss2D(uShadowMap, p.xy, p.z, uShadowBias, uLightSize, uShadowWorldWidth, 8, 12, 0.0)
        : 1.0;                       // PCSS: soft penumbra from the blocker distance
    FragColor = vec4(vec3(sh), 1.0);
})GLSL";
```

`kSkFuncs` provides: `pcfPoisson2D/Array` (Vogel-disk PCF), `pcss2D` (blocker search +
penumbra-sized filter), `vsm2D/Array` + `vsmMomentsFrag` (Chebyshev), `cubeShadow` /
`cubeShadowMoments` for point lights, `shadowFactorArray` (cascade select +
**cross-fade** near the splits via `blendFrac`), `shadowBiasSlope` + normal-offset helper
`shadowWorldPos`, and `contactShadow` (screen-space raymarch against a scene depth texture).
The depth passes are ready-made programs: `kSkDepthVert + kSkDepthFrag` (hard) or
`kSkDepthVert + kSkMomentsFrag` (VSM moments).

## 3. DeferredRenderer on top of the kit (F46)

The deferred lighting pass is assembled with `FUHGL_SHADOW_KIT` and exposes the features
through plain fields (defaults = legacy behavior, forward pipeline untouched):

```cpp
fuhgl::render::DeferredRenderer renderer;
renderer.create(1280, 720);
renderer.enableCsmShadows(2048, 4);
renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 12.f);

renderer.shadowFilter = fuhgl::render::ShadowFilter::Vsm; // or PoissonPcf / Pcss
renderer.shadowSnapToTexels = true;    // stable cascade edges (no swimming)
renderer.shadowCascadeBlend = 0.25f;   // cross-fade 25% of each cascade span
renderer.shadowLightSize = 1.5f;       // PCSS: world-space light size
renderer.shadowMinVariance = 5e-4f;    // VSM: light-bleeding clamp

renderer.enablePointShadow(1, 1024);   // lights[1] must be a point light (cube map)
renderer.setPointShadowRange(0.5f, 60.f);

renderer.contactShadowsEnabled = true; // screen-space contact shadows (non-MSAA only)
renderer.contactShadowMaxDist = 3.f;
renderer.contactShadowThickness = 0.2f;
renderer.contactShadowSteps = 16;
```

All of these are also reachable through the F40 toggle API:
`setEffectEnabled(Effect::PointShadows / Effect::ContactShadows, on)`.
`ShadowFilter::Vsm` automatically creates the moments attachments
(`DeferredRenderer::syncShadowTargets()` recreates the targets when the filter changes).
Contact shadows need a sampleable G-buffer depth — `DeferredPipeline::ensureSampleableDepth()`
switches the depth attachment to a texture (skipped automatically with MSAA).

## 4. Building your own renderer pass on the kit

```cpp
fuhgl::render::ShadowAtlas atlas;
atlas.create(2048, 4);                                   // 4 shadow-casting lights
// per frame, per light:
atlas.renderLight(scene, lightIndex, scene.lights[lightIndex],
                  sceneCenter, sceneRadius, /*snap=*/true);
// in your shader: sample atlas.textureId() as sampler2DArray with layer = lightIndex,
// VP from atlas.vp(lightIndex) — e.g. with pcfPoissonArray() or vsmArray2D() from the kit
```

## Notes

- VSM gives genuinely soft, filterable shadows but can leak light near occluders — tune
  `shadowMinVariance`. PCSS stays on the depth map (no extra attachment) and approximates the
  penumbra from a blocker search; `shadowLightSize` is the world-space light size.
- `snapLightVPToTexels()` should use a stable reference (scene center / cascade center), not
  the camera position.
- Contact shadows darken creases that shadow maps cannot resolve; they complement SSAO,
  they don't replace it.
- Tests: `test_shadow_kit.cpp` (unit: face VPs, texel snapping, kernels) and
  `gl_smoke_shadow_kit.cpp` / `gl_smoke_deferred_kit.cpp` (gl_context: targets + readback,
  kit GLSL compiled and sampled, every filter darkens the shadow blob in the deferred
  renderer, point shadow + contact shadows run without GL errors).
