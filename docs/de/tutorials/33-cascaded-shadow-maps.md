# 33 — Cascaded Shadow Maps (CSM)

> 🇬🇧 **English version:** [docs/en/tutorials/33-cascaded-shadow-maps.md](../../en/tutorials/33-cascaded-shadow-maps.md)

**Ziel:** scharfe Schatten nahe der Kamera *und* in der Distanz — eine Depth-Map pro
Kaskade des Kamera-Frustums statt einer einzigen Map, die entweder nah unscharf oder
weit zu klein ist.

## Nutzung

```cpp
fuhgl::render::ForwardRenderer renderer;
renderer.create(1280, 720);
renderer.enableCsmShadows(2048, 4);          // Map-Größe pro Kaskade, Kaskaden-Anzahl
renderer.setShadowZRange(0.5f, 200.f);       // Kamera-Tiefenbereich der Kaskaden
renderer.setShadowSceneBounds({0, 0.5f, 0}, 12.f);
renderer.addLight(fuhgl::render::Light::directional({0.55f, -1.f, 0.3f}, {1, 1, 1}, 1.5f));
// pro Frame: renderer.render(cam, items, w, h) — der CSM-Pass läuft automatisch
```

Der Deferred-Renderer hat dieselben Schalter: `DeferredRenderer::enableCsmShadows()`.
Auf Pipeline-Ebene: `SceneView::cascades = &map` nach
`CascadeShadowMap::render(scene, camera, light, center, radius, zNear, zFar)`.

## Wie es funktioniert

- `CascadeShadowMap` besitzt ein **Depth-Textur-Array** (`GL_TEXTURE_2D_ARRAY`, eine Ebene
  pro Kaskade). Pro Frame wird die Szenen-Depth einmal pro Kaskade in deren Ebene
  gerendert (`glFramebufferTextureLayer`), mit Back-Faces wie die einzelne `ShadowMap`.
- `cascadeSplits(zNear, zFar, count, lambda)` berechnet die Split-Ebenen mit dem
  Practical Scheme (Log + Uniform, `lambda = 0.5` default).
- `cascadeViewProj(...)` legt eine passend enge Ortho-Box um die 8 Ecken jeder Kamera-
  Frustum-Slice (direktionale Lichter); Spot-Lichter fallen auf die Full-Range-VP zurück.
- Im Shader (`kShadowFunc`) wählt jedes Fragment seine Kaskade über die View-Space-Depth
  (`uCascadeSplits`), projiziert mit der Kaskaden-VP `uLightVPs[i]` und sampelt die
  passende Array-Ebene mit 3x3-PCF über der LINEAR-gefilterten Depth.

## Ergebnis

`gl_smoke_csm` rendert Boden + Werfer mit `enableCsmShadows(1024, 4)` und prüft per
Pixel-Readback, dass der Schatten-Blob **sowohl** in der Nahsicht (Kaskade 0) **als auch**
in der Fernsicht (höhere Kaskade) dunkler wird — der Single-Map-Renderer verliert den
Schatten in der Distanz.

## Hinweise

- `kMaxCascades = 8`; der Single-Map-Pfad (`enableShadows`) ist unverändert und bleibt
  Default — CSM ist opt-in.
- Die Pipelines brauchen die Kamera-View-Matrix zur Kaskaden-Wahl — sie setzen `uView`
  selbst aus `SceneView::camera`.
- Tests: `test_shadow.cpp` (Unit: Split-Monotonie, lambda 0/1, Clamping, VP-Abdeckung pro
  Slice), `gl_smoke_csm.cpp` (Label `gl_context`, skip ohne GL 3.3).
