# 08 — Uniform buffer objects (UBO)

**Goal:** pass whole structs (e.g. camera data) to shaders with one binding instead of
many uniform calls.

```cpp
// GLSL:
//   layout(std140, binding = 0) uniform Camera { mat4 uView; mat4 uProj; };
struct CameraBlock { fuhgl::math::Mat4 view; fuhgl::math::Mat4 proj; };

fuhgl::UniformBuffer ubo; ubo.create();
ubo.upload(cameraBlock);          // allocates + uploads
ubo.bindBase(0);                  // binds to binding point 0

// per frame, only the changing part:
ubo.uploadSub(offsetof(CameraBlock, proj), cam.projection(aspect));
```

`UniformBuffer` wraps a `Buffer` with `Target::Uniform`; use `upload<T>(data)` for the
whole block and `uploadSub<T>(offset, data)` for partial updates.
