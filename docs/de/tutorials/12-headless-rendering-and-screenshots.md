# 12 — Headless-Rendering und Screenshots (EGL surfaceless)

> 🇬🇧 **English version:** [docs/en/tutorials/12-headless-rendering-and-screenshots.md](../../en/tutorials/12-headless-rendering-and-screenshots.md)

**Ziel:** ohne jeglichen Display rendern — wird von der CI für die Tutorial-Screenshots
und den `gl_smoke`-Render-Test benutzt.

EGL-Backend bauen und mit `FUHGL_BACKEND=surfaceless` ausführen:

```sh
cmake -S . -B build-egl -DFUHGL_LINUX_BACKEND=EGL -DCMAKE_BUILD_TYPE=Release
cmake --build build-egl -j
FUHGL_BACKEND=surfaceless ./build-egl/examples/screenshot \
    --out docs/images/tutorial-first-triangle.png --camera fp --width 800 --height 600
FUHGL_BACKEND=surfaceless ./build-egl/examples/screenshot \
    --out docs/images/tutorial-isometric-skybox.png --camera iso --width 800 --height 600
```

`examples/screenshot` erzeugt einen surfaceless-Kontext, rendert die Demo-Szene
(Verlaufs-Skybox + Dreieck) bei festem Zeitschritt (deterministisch) in ein FBO, liest die
Pixel per `gl::readPixels` zurück und schreibt ein PNG mit dem dependency-freien Encoder in
`examples/screenshot/png_writer.hpp`.

Auf Wayland-Desktops öffnet dasselbe EGL-Backend ein echtes xdg-shell-Fenster, wenn
`WAYLAND_DISPLAY` gesetzt ist (und `FUHGL_BACKEND` nicht `surfaceless` ist). X11 bleibt das
Linux-Default-Backend (`-DFUHGL_LINUX_BACKEND=X11`).

Darüber hinaus rendert der CI-Job `docs` alle Tutorial-Screenshots: die Beispiele pro
Thema (`deferred`, `solids`, `lights`, `shadows`, `forward`, `postfx`, `text`,
`instancing`, `gpuinfo`, `stream`, ...) mit `--screenshot`, die Showcase-Looks
(`--mode day|sunset|night`) und die Feature-Szenen von `examples/features`
(`--scene noise|pbr|ibl|csm|taa|ssr|culling|bindless`, Tutorials 30-38). Alle PNGs werden
(plus das Doxygen-HTML) als Artefakte hoch: `tutorial-screenshots` und `doxygen-html`.
