# 08 — Uniform-Puffer-Objekte (UBO)

> 🇬🇧 **English version:** [docs/en/tutorials/08-uniform-buffers.md](../../en/tutorials/08-uniform-buffers.md)

**Ziel:** ganze Strukturen (z. B. Kameradaten) mit einem einzigen Binding an Shader
übergeben, statt vieler einzelner Uniform-Aufrufe.

```cpp
// GLSL:
//   layout(std140, binding = 0) uniform Camera { mat4 uView; mat4 uProj; };
struct CameraBlock { fuhgl::math::Mat4 view; fuhgl::math::Mat4 proj; };

fuhgl::UniformBuffer ubo; ubo.create();
ubo.upload(cameraBlock);          // allokiert + lädt hoch
ubo.bindBase(0);                  // bindet an Bindepunkt 0

// pro Frame nur den Teil, der sich ändert:
ubo.uploadSub(offsetof(CameraBlock, proj), cam.projection(aspect));
```

`UniformBuffer` kapselt einen `Buffer` mit `Target::Uniform`; `upload<T>(data)` lädt den
ganzen Block, `uploadSub<T>(offset, data)` macht partielle Updates.
