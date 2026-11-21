# 35 — Temporal anti-aliasing (TAA)

**Goal:** smooth edges without MSAA: render each frame with a subpixel Halton jitter and
accumulate the history, clamped to the current neighborhood (anti-ghosting).

## Usage

```cpp
fuhgl::render::DeferredRenderer renderer;
renderer.create(1280, 720);
renderer.taaEnabled = true;     // default taaBlend = 0.1
// per frame: renderer.render(scene) — jitter + accumulation happen automatically
```

## How it works (F32)

- Each frame the renderer computes a Halton point (`math::halton(frameIndex)`), converts
  it to an NDC jitter and stores it in `SceneView::jitter`; both pipelines apply it via
  `math::jitterViewProj()` — only the geometry (G-buffer) pass is jittered.
- In the render graph, the `taa` pass is a **feedback** pass: it reads the current HDR
  frame (`ssr` output) and its own output from the previous frame, clamps the history to
  the min/max of the current 3x3 pixel neighborhood, and blends:
  `mix(historyClamped, current, uAlpha)` with `uAlpha = 1` on the first frame and
  `taaBlend` afterwards.
- Bloom, god rays and the composite read the accumulated `taa` texture.

## Result

`gl_smoke_taa` verifies by readback: a single jittered frame differs measurably from the
unjittered one (jitter works), and after 10 accumulated frames the image is significantly
closer to the unjittered reference than the first jittered frame (convergence).

## Notes

- v1 has no velocity-buffer reprojection — fast camera moves can ghost slightly; the
  neighborhood clamp limits it. Static scenes converge cleanly.
- Without `taaEnabled`, `SceneView::jitter` stays 0 and the `taa` pass is a plain copy.
- Test: `tests/gl_smoke_taa.cpp` (label `gl_context`, skips without GL 3.3).
