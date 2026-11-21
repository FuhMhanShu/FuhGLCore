# 35 — Temporal Anti-Aliasing (TAA)

> 🇬🇧 **English version:** [docs/en/tutorials/35-taa.md](../../en/tutorials/35-taa.md)

**Ziel:** glatte Kanten ohne MSAA: jedes Frame mit einem Halton-Subpixel-Jitter rendern
und die History akkumulieren, auf die aktuelle Nachbarschaft geclampt (Anti-Ghosting).

## Nutzung

```cpp
fuhgl::render::DeferredRenderer renderer;
renderer.create(1280, 720);
renderer.taaEnabled = true;     // default taaBlend = 0.1
// pro Frame: renderer.render(scene) — Jitter + Akkumulation laufen automatisch
```

## Wie es funktioniert (F32)

- Pro Frame berechnet der Renderer einen Halton-Punkt (`math::halton(frameIndex)`), rechnet
  ihn in einen NDC-Jitter um und speichert ihn in `SceneView::jitter`; beide Pipelines
  wenden ihn via `math::jitterViewProj()` an — nur der Geometrie-Pass (G-Buffer) wird
  gejittert.
- Im Render-Graph ist der `taa`-Pass ein **Feedback**-Pass: er liest das aktuelle HDR-Frame
  (`ssr`-Output) und seine eigene Ausgabe des vorherigen Frames, clampt die History auf
  min/max der aktuellen 3x3-Pixel-Nachbarschaft und mischt:
  `mix(historyGeclampt, aktuell, uAlpha)` mit `uAlpha = 1` im ersten Frame und `taaBlend` danach.
- Bloom, God Rays und Composite lesen die akkumulierte `taa`-Textur.

## Ergebnis

`gl_smoke_taa` prüft per Readback: ein einzelnes gejittertes Frame unterscheidet sich
messbar vom ungejitterten, und nach 10 akkumulierten Frames ist das Bild deutlich näher am
ungejitterten Referenzbild als das erste gejitterte Frame (Konvergenz).

## Hinweise

- v1 hat keine Velocity-Buffer-Reprojektion — schnelle Kamerabewegungen können leicht
  ghosten; der Nachbarschafts-Clamp begrenzt das. Statische Szenen konvergieren sauber.
- Ohne `taaEnabled` bleibt `SceneView::jitter` 0 und der `taa`-Pass ist eine reine Kopie.
- Test: `tests/gl_smoke_taa.cpp` (Label `gl_context`, skip ohne GL 3.3).
