# 42 — Effect toggles & GPU capability fallback

**Goal:** every effect can be switched on/off through one uniform API on both renderers —
and when the GPU cannot run a feature, it is skipped automatically (with a warning) instead
of failing: the pipelines stay backward-compatible with older graphics cards.

## Usage

```cpp
fuhgl::render::DeferredRenderer renderer;
renderer.create(1280, 720);

// uniform toggle API (same on ForwardRenderer)
renderer.setEffectEnabled(fuhgl::render::Effect::Ssao, false);
renderer.setEffectEnabled(fuhgl::render::Effect::DoF, true);
renderer.setAllEffects(true);            // everything on (still capability-gated)
bool on = renderer.effectEnabled(fuhgl::render::Effect::Taa); // requested AND supported

// what can this GPU do at all?
if (renderer.effectSupported(fuhgl::render::Effect::GpuCulling)) { /* GL >= 4.3 / ARB compute */ }

// the classic field API still works and is routed through the same gating:
renderer.dofEnabled = true;   // == setEffectEnabled(Effect::DoF, true)
```

## How it works (F39/F40)

- `render::Effect` (render/effects.hpp) lists all toggleable features: `Shadows`,
  `CsmShadows`, `Ssao`, `Ibl`, `Bloom`, `GodRays`, `Taa`, `Ssr`, `DoF`, `VolFog`, `Lod`,
  `HeightmapTerrain`, `GpuCulling`, `BindlessTextures`.
- `render::effectSupported(Effect, glMajor, glMinor, hasBindless, hasCompute)` is a **pure**
  mapping (unit-tested without GL): all built-in shaders are GLSL 330, so the GPU effects
  need GL ≥ 3.3 (CSM additionally texture arrays, core since 3.0); GPU culling needs compute
  shaders (GL ≥ 4.3 or `GL_ARB_compute_shader`); bindless needs `GL_ARB_bindless_texture`;
  LOD and heightmap terrain are CPU-side and always supported.
- Both renderers capture `gl::queryDeviceInfo()` in `create()` (version, `GL_ARB_bindless_texture`,
  compute flag — `gl::Features` gained `textureArrays`/`bindlessTextures`).
- `applyCapabilities()` runs at the start of every `render()`: any enabled-but-unsupported
  effect flag is cleared with a one-time `warnUnsupported()` warning, and an IBL environment
  is dropped the same way. `effectEnabled(e)` returns `requested && supported`, so user code
  can always rely on it.
- `setEffectEnabled()` refuses unsupported effects with a warning; the existing public
  fields (`dofEnabled`, `taaEnabled`, …) remain valid — same storage, same gating.

## Notes

- The graceful-degradation path is exercised by `test_effects.cpp` (mapping matrix) and
  `gl_smoke_effects.cpp` (label `gl_context`: toggle round-trips on both renderers, a frame
  with all effects on, no GL errors). On a GL 3.3 context everything except culling/bindless
  is available; on GL 4.3+ everything is.
- Standalone GPU features degrade the same way: `GpuCuller::create()` returns false below
  GL 4.3, `gl::ext::BindlessTexture` handles are 0 without the extension.
