# 21 — Shadow mapping (F15)

**Goal:** the prerequisites for real-time shadows: a `render::ShadowMap` that renders
the scene depth from a light's point of view (depth-only pass), helpers to build the
light view-projection matrix, and PCF shadow sampling wired into the pipelines'
built-in lighting shaders.

![shadows](images/tutorial-shadows.png)

## The pieces

### 1. Light view-projection (`render/shadow.hpp`)

```cpp
fuhgl::render::Light sun = fuhgl::render::Light::directional({0.55f, -1.f, 0.3f}, {1.f, 0.9f, 0.72f}, 1.7f);

// directional light: ortho box of `radius` around `center`, looking along -direction
// spot light:        perspective with fov = 2 * coneOuter at the light position
fuhgl::math::Mat4 lightVP = fuhgl::render::lightViewProj(sun, /*sceneCenter*/ {0.f, 0.5f, 0.f}, /*radius*/ 8.f);
// or the individual matrices:
fuhgl::math::Mat4 lv = fuhgl::render::lightView(sun, center, radius);
fuhgl::math::Mat4 lp = fuhgl::render::lightProjection(sun, /*aspect*/ 1.f);
```

Straight-down lights are handled (the up-vector flips to +X to avoid a degenerate
`lookAt`). Point-light shadows would need a cube shadow map — not covered here.

### 2. The shadow map (`render::ShadowMap`, PIMPL — owns a depth texture + FBO)

```cpp
fuhgl::render::ShadowMap shadowMap;
shadowMap.create(1024);                    // depth texture 1024x1024, LINEAR filtered

// per frame, before the main render: depth pass from the light's POV
shadowMap.render(scene, lightVP);          // back faces are drawn (less acne)
```

`ShadowMap::render()` binds its depth-only FBO (`glDrawBuffer(GL_NONE)`), clears
depth and draws every `scene.items` entry with a minimal depth program
(`uModel`, `uLightVP`).

### 3. Sampling in the lighting pass

Set the map on the `SceneView`; both `ForwardPipeline` and `DeferredPipeline` pick it
up automatically:

```cpp
scene.shadowMap = &shadowMap;
scene.lightVP = lightVP;
scene.shadowLightIndex = 0;   // index into scene.lights[] the map belongs to
scene.shadowStrength = 1.f;   // 0 = off, 1 = full
pipeline.render(scene);
```

The shared GLSL (`src/render/lighting_glsl.hpp`, `kShadowFunc`) projects the fragment
into light space, applies a slope-scaled depth bias and does a 3×3 PCF on top of the
LINEAR-filtered depth texture:

```glsl
float shadowFactor(vec3 worldPos, vec3 N, vec3 L) {
    vec4 lp = uLightVP * vec4(worldPos, 1.0);
    vec3 p = lp.xyz / lp.w * 0.5 + 0.5;
    if (p.z > 1.0 || p.xy outside [0,1]) return 1.0;   // outside the map = lit
    float bias = max(0.0015 * (1.0 - dot(N, L)), 0.0004);
    // 3x3 taps, texture(uShadowMap, p.xy + offset).r >= p.z - bias
}
```

Only `uShadowLightIndex` is shadowed — the other lights stay unshadowed (one shadow
map per frame keeps it cheap).

## Complete example

See `examples/shadows/main.cpp`: floor + three boxes + one sun, depth pass then
forward pass. The headless test `tests/gl_smoke_shadows.cpp` renders the scene with
and without the shadow map and asserts that a visible blob of pixels gets darker and
nothing gets brighter. `tests/test_shadow.cpp` unit-tests the light VP math (scene
sphere stays inside the ortho box, spot cone edge maps to |ndc| ≈ 1, no NaN for
straight-down lights).

## Tuning knobs

| Knob | Where | Effect |
|---|---|---|
| map size | `shadowMap.create(size)` | resolution / shadow edge quality |
| `sceneRadius` in `lightViewProj` | per frame | ortho box extent — must cover the shadowed area |
| bias | `kShadowFunc` in `lighting_glsl.hpp` | acne vs. peter-panning |
| back-face depth pass | `ShadowMap::render` | `cullFace(Front)` reduces acne |
| `shadowStrength` | `SceneView` | 0..1 softness of the shadow |
