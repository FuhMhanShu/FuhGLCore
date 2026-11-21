# 12 — Headless rendering and screenshots (EGL surfaceless)

**Goal:** render without any display — used by CI for the tutorial screenshots and the
`gl_smoke` render test.

Build the EGL backend and run with `FUHGL_BACKEND=surfaceless`:

```sh
cmake -S . -B build-egl -DFUHGL_LINUX_BACKEND=EGL -DCMAKE_BUILD_TYPE=Release
cmake --build build-egl -j
FUHGL_BACKEND=surfaceless ./build-egl/examples/screenshot \
    --out docs/images/tutorial-first-triangle.png --camera fp --width 800 --height 600
FUHGL_BACKEND=surfaceless ./build-egl/examples/screenshot \
    --out docs/images/tutorial-isometric-skybox.png --camera iso --width 800 --height 600
```

`examples/screenshot` creates a surfaceless context, renders the demo scene
(gradient skybox + triangle) into an FBO at a fixed time step (deterministic),
reads the pixels back with `gl::readPixels` and writes a PNG using the dependency-free
encoder in `examples/screenshot/png_writer.hpp`.

On Wayland desktops the same EGL backend opens a real xdg-shell window when
`WAYLAND_DISPLAY` is set (and `FUHGL_BACKEND` is not `surfaceless`). X11 remains the
default Linux backend (`-DFUHGL_LINUX_BACKEND=X11`).

The CI `docs` job renders both screenshots and uploads them (plus the Doxygen HTML) as
artifacts: `tutorial-screenshots` and `doxygen-html`.
