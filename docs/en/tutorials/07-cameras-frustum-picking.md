# 07 — Cameras, frustum culling and picking

**Goal:** fly around with ready-made cameras, cull with a frustum, pick with a ray.

```cpp
fuhgl::FirstPersonCamera fp;  fp.position = {0,0,3}; fp.attach(window); // WASD+mouse, Shift=sprint, Space/Ctrl up/down
fuhgl::FreeCamera free;       free.attach(window);                       // 6-DOF, Q/E roll
fuhgl::IsometricCamera iso;   iso.attach(window);                        // ortho, RMB-drag pan, wheel zoom, arrows

// all cameras share the interface:
fuhgl::Camera* cam = &fp;
cam->update(dt);
fuhgl::math::Mat4 view = cam->view();
fuhgl::math::Mat4 proj = cam->projection(width / (float)height);
fuhgl::math::Mat4 viewProj = cam->viewProj(aspect);   // convenience
```

## Frustum culling

```cpp
fuhgl::math::Frustum fr = fuhgl::math::Frustum::fromViewProj(viewProj);
if (fr.containsSphere(objectCenter, objectRadius)) { /* draw */ }
```

## Picking (mouse → world ray)

```cpp
fuhgl::math::Ray r = fuhgl::math::rayFromScreen(viewProj, mouseX, mouseY, width, height);
// intersect r with your scene (ray vs. triangle / AABB / sphere)
```

All of this is pure math — unit-tested headless in `tests/test_frustum.cpp`.
