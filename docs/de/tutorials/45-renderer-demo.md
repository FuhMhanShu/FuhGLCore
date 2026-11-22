# 45 — Renderer-Demo: eine Szene, beide Renderer

> 🇬🇧 **English version:** [docs/en/tutorials/45-renderer-demo.md](../../../en/tutorials/45-renderer-demo.md)

**Ziel:** die Render-Schicht *als Demo* betreiben — eine fertige Szene, die mit **beiden**
 einsatzbereiten Renderern funktioniert. `render::DemoScene` (`render/demo.hpp`) baut die
Showcase-Landschaft einmal auf (Terrain, CSG-Props, instanziertes Gras, Lichter, drei Looks);
`configure()` passt den passenden Effekt-Stack an einen `ForwardRenderer` oder
`DeferredRenderer` an, `fillScene()` liefert die `SceneView` pro Frame. `examples/demo` führt
dieselbe Szene durch beide Renderer:

```
./build/examples/demo --renderer deferred --mode day     # DeferredRenderer, voller FX-Stack
./build/examples/demo --renderer forward  --mode sunset  # ForwardRenderer, HDR-Postkette
```

![Demo-Szene, Deferred-Renderer](../../images/tutorial-renderer-demo-deferred.png)
![Demo-Szene, Forward-Renderer mit Postkette](../../images/tutorial-renderer-demo-forward.png)

Dieses Tutorial ist der zusammenfassende Teil: Es ordnet jedes Einzel-Tutorial der Demo zu —
so sieht man auf einen Blick, wo jedes Feature in einem laufenden Programm landet.

## Verwendung

```cpp
fuhgl::render::DemoScene demo;
demo.create(fuhgl::render::DemoMode::Day);   // Day | Sunset | Night (braucht GL-Kontext)

fuhgl::render::DeferredRenderer renderer;    // ... oder fuhgl::render::ForwardRenderer
renderer.create(w, h);
demo.configure(renderer);                    // CSM-Schatten + passender Post-FX-Stack + Licht/Nebel/Sky

fuhgl::FirstPersonCamera cam;
demo.applyStartPose(cam);

fuhgl::render::SceneView scene;
scene.camera = &cam;
scene.width = w; scene.height = h; scene.aspect = float(w) / h;
demo.fillScene(scene);                       // Items + Lichter + Nebel + Clear-Farbe (+ IBL bei Nacht)

renderer.render(scene);                      // deferred; forward: renderer.render(cam, scene.items, w, h)
```

Headless-Screenshot (so rendert die CI die beiden Bilder oben):

```
FUHGL_BACKEND=surfaceless ./build-egl/examples/demo --screenshot tutorial-renderer-demo-deferred.png --renderer deferred --mode day --width 960 --height 540
FUHGL_BACKEND=surfaceless ./build-egl/examples/demo --screenshot tutorial-renderer-demo-forward.png  --renderer forward  --mode day --width 960 --height 540
```

`examples/showcase` ist der Deferred-only-Dünn-Wrapper über dieselbe `DemoScene`
(README-Hero-Bilder, Tutorial 29) — der Szenen-Code ist nicht mehr doppelt vorhanden.

## Welches Tutorial landet wo in der Demo

| Tutorial | Feature | In der `DemoScene`-Demo |
|---|---|---|
| 06 | Skybox | Farbverlauf-Sky in der Deferred-Lighting-Pass (`skyTop/skyBottom`); Nachtmodus backt seine IBL-Quelle aus einer Verlauf-Cubemap |
| 17 + 18 | Prozedurale Texturen, CSG-Solids | Voronoi-/Noise-Texturen auf Fels/Stein/Gras; Bäume, Büsche, Felsen, Monolith als CSG-gebaute Meshes |
| 20 | Lichter | direktionales Sonnenlicht (je Modus) + Punktlicht; Nacht ergänzt emissive Lampen |
| 21 + 33 | Schatten / CSM | `enableCsmShadows(2048, 4)` auf **beiden** Renderern über `configure()` |
| 22 + 23 | Forward- / Deferred-Renderer | die beiden Renderer, zwischen denen `examples/demo --renderer ...` wechselt |
| 24 | Text-HUD | FPS-/Effekt-Zeile in beiden Beispielen |
| 28 | Instancing | 3000 Grashalme in 2 instanzierten Draws |
| 29 | Showcase-Szene | diese Demo — in die Bibliothek als `DemoScene` gewandert |
| 31 | PBR-Materialien | texturiertes Terrain (`pbrTextured`), metallisch-glatter Teich (`pbr`) |
| 32 | IBL | Nachtmodus: `Environment::createFromCubemap` + `scene.env` |
| 35 + 36 | TAA, SSR | deferred: TAA auf dem HDR-Bild, SSR auf dem Teich; forward: TAA über PostChain (SSR nur deferred) |
| 39 + 40 | DoF, volumetrischer Nebel | beide Renderer (forward rekonstruiert Tiefe aus der Depth-Textur, Tutorial 44) |
| 41 | Heightmap-Terrain | Noise-Heightfield-Mesh + `heightAt()` für Platzierung und Kamera |
| 42 | Effekt-Schalter + Capability-Fallback | alle Demo-Effekte laufen über die gated Renderer-APIs |
| 44 | Forward-Postkette | `configure(ForwardRenderer&)` schaltet `enablePostFX()` + Bloom/God Rays/TAA/DoF/Vol-Nebel |

## Wie es funktioniert (F43)

- `DemoScene` (PIMPL, `src/render/demo.cpp`) besitzt alle GL-Objekte der Szene — Meshes,
  Texturen, Materialien, die Nacht-IBL-Umgebung. `create(mode)` baut sie einmal; die von
  `fillScene()` gefüllte `SceneView` hält nur Zeiger, darf die `DemoScene` also nicht überleben.
- `configure(ForwardRenderer&)` / `configure(DeferredRenderer&)` sind der „Renderer als Demo“-Teil:
  gleiche Szene, renderer-spezifischer Effekt-Stack — CSM + Lichter + Nebel + Ambient + Clear
  auf beiden; deferred zusätzlich SSAO/SSR und den Verlauf-Sky, forward `enablePostFX()` mit
  der PostChain (Bloom, God Rays, TAA, DoF, Vol-Nebel).
- Drei Looks über `DemoMode`: **Day** (hohe Sonne, Morgennebel), **Sunset** (Sonne tief im
  Bild → lange Schatten + starke God Rays), **Night** (dunkler Sky + gebakte IBL + emissive
  Lampen, im Teich per SSR gespiegelt).
- Deterministisch: jegliches Streuen nutzt geseedete Hashes, Headless-CI-Screenshots sind
  reproduzierbar; die TAA-Historie akkumuliert über 8 Headless-Frames.

## Hinweise

- Kosten: die Demo ist der volle Effekt-Stack — auf schwachen GPUs schaltet das F39-Capability-
  Gating nicht unterstützte Effekte mit Warnung ab (Tutorial 42).
- Test: `gl_smoke_demo.cpp` (Label `gl_context`) rendert die Day-Szene headless durch **beide**
  Renderer und prüft beleuchtetes Bild + Wiring (CSM an, Post-FX an, Lichter gesetzt, Nacht-IBL
  vorhanden) + keine GL-Fehler.
- `demoModeFromName("sunset")` parst `--mode` (Groß-/Kleinschreibung egal).
