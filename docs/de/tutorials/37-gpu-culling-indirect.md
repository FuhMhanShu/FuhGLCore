# 37 — GPU-Culling + Indirect Draw

> 🇬🇧 **English version:** [docs/en/tutorials/37-gpu-culling-indirect.md](../../en/tutorials/37-gpu-culling-indirect.md)

**Ziel:** Tausende Instanzen eines Meshes mit einem einzigen Draw-Call rendern, während
die GPU entscheidet, welche Instanzen sichtbar sind — keine Pro-Instanz-Arbeit auf der
CPU, kein Overdraw durch Off-Screen-Instanzen.

## Verwendung

```cpp
fuhgl::render::Mesh boxMesh;
fuhgl::render::shapes::box(boxMesh, {0.3f, 0.3f, 0.3f}, {1.f, 1.f, 1.f});

fuhgl::render::GpuCuller culler;
culler.create(4096);            // Kapazität in Instanzen, braucht GL >= 4.3
culler.setMesh(boxMesh);        // Mesh, das pro sichtbarer Instanz gezeichnet wird

std::vector<fuhgl::math::Mat4> models = /* ... eine Model-Matrix pro Instanz ... */;
culler.setInstances(models.data(), (int)models.size());

// pro Frame:
culler.cull(camera.viewProj(aspect));   // Compute-Shader: Frustum-Cull + Kompaktierung
prog.use();                              // Programm liest aInstanceModel (Loc 4..7)
prog.setUniform("uViewProj", camera.viewProj(aspect));
culler.draw();                           // EIN indirekter instanzierter Draw
int visible = culler.lastVisibleCount();
```

## Wie es funktioniert (F34)

`render::GpuCuller` (`render/culling.hpp`, PIMPL) besitzt drei Buffer:

1. **Items-SSBO** (Binding 0): pro Instanz eine `mat4 model` + `vec4 sphere` (die
   objektseitige Bounding Sphere des Meshes, `Mesh::boundingSphere()`).
2. **Models-SSBO** (Binding 1): die *kompaktierten* Model-Matrizen der sichtbaren
   Instanzen. Derselbe Buffer ist zugleich als `GL_ARRAY_BUFFER` an die Divisor-1-`mat4`-
   Vertex-Attribute an den Locations 4..7 gebunden — exakt das Layout von
   `Mesh::setInstanceData()`, bestehende instanzierte Shader (`aInstanceModel`) laufen
   unverändert.
3. **Indirect-Command-Buffer** (Binding 2, `GL_DRAW_INDIRECT_BUFFER`): ein
   `DrawElementsIndirectCommand`/`DrawArraysIndirectCommand`, dessen `count`/`firstIndex`
   einmalig von `setMesh()` gesetzt werden, plus ein atomarer Zähler.

Der Compute-Shader (`#version 430`, 64 Threads/Gruppe) macht pro Instanz:

- World-Sphere = `math::transformSphere(model, sphere)` — Zentrum über die Matrix, Radius
  skaliert mit der größten Achsenskalierung (CPU-Spiegel in `math::transformSphere`,
  unit-getestet in `test_culling.cpp`),
- Frustum-Test gegen die 6 Ebenen von `math::Frustum::fromViewProj(viewProj)`
  (Sphere-vs-Plane: `dot(n, c) + d < -r` → gecullt),
- sichtbare Instanzen: `slot = atomicAdd(counter, 1)`, `models[slot] = model`,
  `atomicMax(command.instanceCount, slot + 1)`.

`draw()` bindet dann das VAO des Cullers (Mesh-Vertexdaten + kompaktierte Instanzmatrizen)
und issued `glDrawElementsIndirect` / `glDrawArraysIndirect` — ein Draw, `instanceCount`
von der GPU geschrieben.

## Ergebnis

`gl_smoke_culling` platziert 1000 Box-Instanzen (500 vor, 500 hinter der Kamera), cullt
sie auf der GPU und prüft: Der Zähler liest sich als exakt 500 zurück, `instanceCount` des
indirekten Commands ist 500, der indirekte Draw zeichnet Pixel — und eine wegblickende
Kamera cullt alles (0 sichtbar, 0 Pixel).

## Hinweise

- Braucht GL 4.3 (Compute-Shader) — `create()` liefert sonst false; macOS (GL 4.1) ist
  über den CPU-Spiegel (`math::transformSphere` + `Frustum::containsSphere`) und die
  Unit-Tests abgedeckt.
- Culling ist pro *instanziertem Mesh-Set*: für viele verschiedene Meshes einen Culler
  pro Mesh verwenden (oder Items nach Mesh gruppieren) — die API bleibt bewusst schlank.
- Tests: `tests/test_culling.cpp` (Unit), `tests/gl_smoke_culling.cpp` (Label
  `gl_context`, skippt ohne GL 4.3).
