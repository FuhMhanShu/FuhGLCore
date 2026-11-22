# 45 — Shadow-Kit: technische Voraussetzungen für realistische Licht- und Schattenwürfe (F45/F46)

> 🇬🇧 **English version:** [docs/en/tutorials/45-shadow-kit.md](../../en/tutorials/45-shadow-kit.md)

**Ziel:** die *technischen Voraussetzungen* für realistischen Licht- und Schattenwurf als
wiederverwendbare Bausteine — plus der darauf aufbauende `DeferredRenderer`. Das Kit selbst
ist bewusst **kein** Renderer-Feature: Man baut es in eigene Pässe und Shader ein.

## 1. Die Bausteine (`render/shadow_kit.hpp`)

| Baustein | Was er liefert |
|---|---|
| `render::ShadowTarget` | generisches Depth-Target — `Tex2D`, `Tex2DArray`, `Cube` — mit optionaler RGBA32F-**Momenten**-Attachment (`create(kind, size, layers, moments=true)`) für VSM; `render(scene, vp, layerOrFace)` zeichnet die Szenen-Tiefe (Backfaces, wie `ShadowMap`) |
| `render::CubeShadowMap` | Cube-Schatten-Map für **ein Punktlicht**: `create(size, moments)`, `render(scene, pointLight, zNear, zFar)` füllt alle 6 Faces, `faceVP(i)`, `farPlane()` |
| `render::ShadowAtlas` | **Mehrfach-Schatten**: eine 2D-Array-Ebene pro werfendem Licht; `renderLight(scene, layer, light, center, radius, snap)` speichert die (optional gesnappte) Licht-VP pro Ebene |
| `pointLightFaceVPs(pos, near, far)` | die 6 Cube-Face-View-Projektionen eines Punktlichts |
| `snapLightVPToTexels(vp, mapSize, refWorldPos)` | **Kaskaden-Stabilisierung**: verschiebt die Licht-VP so, dass ein Referenzpunkt auf eine Texelmitte landet — kills „Schwimmen" der Schatten bei Kamerabewegung |
| `hemisphereKernel(n, seed)` / `poissonDisk(n, seed)` | deterministische Sample-Muster für SSAO/HBAO/Contact-Shadow-Kernel und PCF/PCSS (CPU-seitig, unit-testbar) |

`ShadowMap` und `CascadeShadowMap` sind jetzt dünne Wrapper über `ShadowTarget` und haben
dadurch Momenten-Support gewonnen (`create(size, moments)`, `momentsTextureId()`) sowie
`CascadeShadowMap::snapToTexels` — die öffentliche API ist sonst unverändert.

## 2. Das GLSL-Kit (`render/shadow_glsl.hpp`)

Zusammensetzbare `inline constexpr`-Strings (GLSL 330) — eigener Shader daraus:

```cpp
std::string fs = std::string("#version 330 core\nout vec4 FragColor;\n")
               + fuhgl::render::kSkUniforms + fuhgl::render::kSkFuncs
               + R"GLSL(
void main() {
    vec3 p = shadowNdc(uLightVP, vWorldPos);
    float sh = shadowInBounds(p)
        ? pcss2D(uShadowMap, p.xy, p.z, uShadowBias, uLightSize, uShadowWorldWidth, 8, 12, 0.0)
        : 1.0;                       // PCSS: weiche Penumbra aus der Blocker-Distanz
    FragColor = vec4(vec3(sh), 1.0);
})GLSL";
```

`kSkFuncs` bietet: `pcfPoisson2D/Array` (Vogel-Disk-PCF), `pcss2D` (Blocker-Search +
penumbra-großer Filter), `vsm2D/Array` + `kSkMomentsFrag` (Chebyshev), `cubeShadow` /
`cubeShadowMoments` für Punktlichter, `shadowFactorArray` (Kaskadenwahl + **Cross-Fade**
nahe an den Splits über `blendFrac`), `shadowBiasSlope` + Normal-Offset-Helfer
`shadowWorldPos` sowie `contactShadow` (Screen-Space-Raymarch gegen eine Szenen-Depth-Textur).
Die Depth-Pässe sind fertige Programme: `kSkDepthVert + kSkDepthFrag` (hart) bzw.
`kSkDepthVert + kSkMomentsFrag` (VSM-Momenten).

## 3. DeferredRenderer auf dem Kit (F46)

Der Deferred-Lighting-Pass wird mit `FUHGL_SHADOW_KIT` assembliert und bietet die Features
über einfache Felder (Defaults = Legacy-Verhalten, Forward-Pipeline unverändert):

```cpp
fuhgl::render::DeferredRenderer renderer;
renderer.create(1280, 720);
renderer.enableCsmShadows(2048, 4);
renderer.setShadowSceneBounds({0.f, 0.5f, 0.f}, 12.f);

renderer.shadowFilter = fuhgl::render::ShadowFilter::Vsm; // oder PoissonPcf / Pcss
renderer.shadowSnapToTexels = true;    // stabile Kaskaden-Kanten (kein Schwimmen)
renderer.shadowCascadeBlend = 0.25f;   // Cross-Fade über 25% jeder Kaskaden-Spanne
renderer.shadowLightSize = 1.5f;       // PCSS: Lichtgröße in Welt-Einheiten
renderer.shadowMinVariance = 5e-4f;    // VSM: Light-Bleeding-Clamp

renderer.enablePointShadow(1, 1024);   // lights[1] muss ein Punktlicht sein (Cube-Map)
renderer.setPointShadowRange(0.5f, 60.f);

renderer.contactShadowsEnabled = true; // Screen-Space Contact Shadows (nur ohne MSAA)
renderer.contactShadowMaxDist = 3.f;
renderer.contactShadowThickness = 0.2f;
renderer.contactShadowSteps = 16;
```

Alles ist zusätzlich über die F40-Toggle-API erreichbar:
`setEffectEnabled(Effect::PointShadows / Effect::ContactShadows, on)`.
`ShadowFilter::Vsm` legt die Momenten-Attachments automatisch an
(`DeferredRenderer::syncShadowTargets()` erstellt die Targets neu, wenn der Filter wechselt).
Contact Shadows brauchen eine samplebare G-Buffer-Depth —
`DeferredPipeline::ensureSampleableDepth()` schaltet die Depth-Attachment auf eine Textur um
(bei MSAA automatisch übersprungen).

## 4. Eigener Render-Pass auf dem Kit

```cpp
fuhgl::render::ShadowAtlas atlas;
atlas.create(2048, 4);                                   // 4 werfende Lichter
// pro Frame, pro Licht:
atlas.renderLight(scene, lightIndex, scene.lights[lightIndex],
                  sceneCenter, sceneRadius, /*snap=*/true);
// im Shader: atlas.textureId() als sampler2DArray mit layer = lightIndex samplen,
// VP aus atlas.vp(lightIndex) — z.B. mit pcfPoissonArray() oder vsmArray2D() aus dem Kit
```

## Hinweise

- VSM liefert echt weiche, filterbare Schatten, kann aber nahe an Occludern Licht lecken —
  `shadowMinVariance` tunen. PCSS bleibt auf der Depth-Map (kein extra Attachment) und
  nähert die Penumbra über eine Blocker-Suche an; `shadowLightSize` ist die Lichtgröße in
  Welt-Einheiten.
- `snapLightVPToTexels()` sollte einen stabilen Referenzpunkt nutzen (Szenen-/Kaskaden-Mitte),
  nicht die Kameraposition.
- Contact Shadows verdunkeln Spalten, die Schatten-Maps nicht auflösen können; sie ergänzen
  SSAO, ersetzen es nicht.
- Tests: `test_shadow_kit.cpp` (Unit: Face-VPs, Texel-Snapping, Kernel) und
  `gl_smoke_shadow_kit.cpp` / `gl_smoke_deferred_kit.cpp` (gl_context: Targets + Readback,
  Kit-GLSL kompiliert und gesamplet, jeder Filter verdunkelt den Schattenfleck im
  Deferred-Renderer, Punktlicht-Schatten + Contact Shadows laufen fehlerfrei).
