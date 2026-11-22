# 42 — Effekt-Schalter & GPU-Kompatibilitäts-Fallback

> 🇬🇧 **English version:** [docs/en/tutorials/42-effects-capabilities.md](../../en/tutorials/42-effects-capabilities.md)

**Ziel:** Jedes Feature lässt sich über eine einheitliche API an beiden Renderern an- und
abschalten — und wenn die GPU ein Feature nicht kann, wird es automatisch übersprungen
(mit Warnung) statt zu fehlschlagen: Die Pipelines bleiben abwärtskompatibel.

## Verwendung

```cpp
fuhgl::render::DeferredRenderer renderer;
renderer.create(1280, 720);

// einheitliche Schalter-API (an ForwardRenderer identisch)
renderer.setEffectEnabled(fuhgl::render::Effect::Ssao, false);
renderer.setEffectEnabled(fuhgl::render::Effect::DoF, true);
renderer.setAllEffects(true);            // alles an (weiterhin capability-gegated)
bool an = renderer.effectEnabled(fuhgl::render::Effect::Taa); // angefragt UND unterstützt

// was kann diese GPU überhaupt?
if (renderer.effectSupported(fuhgl::render::Effect::GpuCulling)) { /* GL >= 4.3 / ARB Compute */ }

// die klassische Feld-API funktioniert weiter und läuft durch dasselbe Gating:
renderer.dofEnabled = true;   // == setEffectEnabled(Effect::DoF, true)
```

## So funktioniert es (F39/F40)

- `render::Effect` (render/effects.hpp) listet alle schaltbaren Features: `Shadows`,
  `CsmShadows`, `Ssao`, `Ibl`, `Bloom`, `GodRays`, `Taa`, `Ssr`, `DoF`, `VolFog`, `Lod`,
  `HeightmapTerrain`, `GpuCulling`, `BindlessTextures`.
- `render::effectSupported(Effect, glMajor, glMinor, hasBindless, hasCompute)` ist eine
  **reine** Abbildung (ohne GL unit-getestet): Alle eingebauten Shader sind GLSL 330, die
  GPU-Effekte brauchen also GL ≥ 3.3 (CSM zusätzlich Texture-Arrays, seit 3.0 Core);
  GPU-Culling braucht Compute-Shader (GL ≥ 4.3 oder `GL_ARB_compute_shader`); Bindless
  braucht `GL_ARB_bindless_texture`; LOD und Heightmap-Terrain sind CPU-seitig und immer
  verfügbar.
- Beide Renderer erfassen in `create()` die `gl::queryDeviceInfo()` (Version,
  `GL_ARB_bindless_texture`, Compute-Flag — `gl::Features` hat `textureArrays`/
  `bindlessTextures` gained).
- `applyCapabilities()` läuft zu Beginn jedes `render()`: jedes aktivierte, aber nicht
  unterstützte Feature wird mit einmaliger `warnUnsupported()`-Warnung abgeschaltet; eine
  IBL-Umgebung wird ebenso fallengelassen. `effectEnabled(e)` liefert `angefragt &&
  unterstützt`.
- `setEffectEnabled()` lehnt nicht unterstützte Effekte mit Warnung ab; die bisherigen
  öffentlichen Felder (`dofEnabled`, `taaEnabled`, …) bleiben gültig — derselbe Speicher,
  dasselbe Gating.

## Hinweise

- Der Graceful-Degradation-Pfad wird von `test_effects.cpp` (Mapping-Matrix) und
  `gl_smoke_effects.cpp` (Label `gl_context`: Schalter-Roundtrips an beiden Renderern, ein
  Frame mit allen Effekten, keine GL-Fehler) abgedeckt. Auf GL 3.3 ist alles außer
  Culling/Bindless verfügbar, ab GL 4.3 alles.
- Eigenständige GPU-Features degradieren ebenso: `GpuCuller::create()` liefert unter
  GL 4.3 false, `gl::ext::BindlessTexture`-Handles sind ohne Extension 0.
