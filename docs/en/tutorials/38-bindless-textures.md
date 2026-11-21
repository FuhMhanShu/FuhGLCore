# 38 — Bindless textures (GL_ARB_bindless_texture)

**Goal:** sample textures in shaders without binding them to texture units — a 64-bit
handle in a `uint64_t` uniform is enough. This removes the texture-unit bottleneck and
lets a shader access arbitrary texture arrays (e.g. a bindless material library).

![bindless texture sampled through a 64-bit handle](../../images/tutorial-bindless.png)

## Usage

```cpp
fuhgl::Texture albedo(fuhgl::Texture::Target::Tex2D);
albedo.bind();
albedo.setImage2D(0, GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

fuhgl::gl::ext::BindlessTexture handle;
if (handle.attach(albedo)) {            // makes the handle resident
    prog.use();
    fuhgl::gl::ext::setUniformHandle(prog, "uTex", handle.handle());
    // ... draw ...
}                                       // destructor: non-resident again
```

Fragment shader:

```glsl
#version 330 core
#extension GL_ARB_bindless_texture : require
uniform uint64_t uTex;
void main() { FragColor = texture(sampler2D(uTex), vUV); }
```

## How it works (F35)

`gl/ext/bindless.hpp` resolves the `GL_ARB_bindless_texture` entry points
(`glGetTextureHandleARB`, `glMakeTextureHandleResidentARB`,
`glMakeTextureHandleNonResidentARB`, `glIsTextureHandleResidentARB`,
`glUniformHandleui64ARB`, `glProgramUniformHandleui64ARB`) via `gl::procAddress()`:

- `ext::supported()` — extension present and all pointers resolved (false on macOS,
  which has no bindless support; code must guard with it),
- `ext::BindlessTexture` — RAII handle: `attach(tex)` creates + makes resident,
  `detach()`/destructor makes non-resident, `handle()`/`isResident()` query,
- `ext::setUniformHandle(prog, name, handle)` — sets the `uint64_t` uniform (via
  `glProgramUniformHandleui64ARB` when available, else `glUniformHandleui64ARB`).

The texture object itself stays owned by the caller; the handle is only valid while the
texture is resident.

## Result

`gl_smoke_bindless` uploads an 8×8 texture of a known color, samples it through a
bindless handle in a fullscreen pass and verifies the center pixel matches; it also
checks `detach()` makes the handle non-resident. Skips with code 77 when the extension is
missing (e.g. macOS).

## Notes

- Handles are 64-bit and opaque; never store them beyond the resident lifetime.
- Combine with F34 (GPU culling): an instance can carry a texture handle and sample it
  without any texture-unit management.
- Test: `tests/gl_smoke_bindless.cpp` (label `gl_context`, skips without the extension).
