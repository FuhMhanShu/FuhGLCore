# 07 — Kameras, Frustum-Culling und Picking

> 🇬🇧 **English version:** [docs/en/tutorials/07-cameras-frustum-picking.md](../../en/tutorials/07-cameras-frustum-picking.md)

**Ziel:** mit fertigen Kameras herumfliegen, per Frustum cullen, per Strahl picken.

```cpp
fuhgl::FirstPersonCamera fp;  fp.position = {0,0,3}; fp.attach(window); // WASD+Maus, Shift=Sprint, Space/Ctrl hoch/runter
fuhgl::FreeCamera free;       free.attach(window);                       // 6-DOF, Q/E für Roll
fuhgl::IsometricCamera iso;   iso.attach(window);                        // ortho, RMB-Ziehen verschiebt, Mausrad zoomt, Pfeiltasten

// alle Kameras teilen das Interface:
fuhgl::Camera* cam = &fp;
cam->update(dt);
fuhgl::math::Mat4 view = cam->view();
fuhgl::math::Mat4 proj = cam->projection(width / (float)height);
fuhgl::math::Mat4 viewProj = cam->viewProj(aspect);   // praktischer Helfer
```

## Frustum-Culling

```cpp
fuhgl::math::Frustum fr = fuhgl::math::Frustum::fromViewProj(viewProj);
if (fr.containsSphere(objectCenter, objectRadius)) { /* zeichnen */ }
```

## Picking (Maus → Weltstrahl)

```cpp
fuhgl::math::Ray r = fuhgl::math::rayFromScreen(viewProj, mouseX, mouseY, width, height);
// r mit der Szene schneiden (Strahl vs. Dreieck / AABB / Kugel)
```

Das alles ist reine Mathematik — headless unit-getestet in `tests/test_frustum.cpp`.
