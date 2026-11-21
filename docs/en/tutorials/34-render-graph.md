# 34 — Render graph: declarative passes, auto-ordered

**Goal:** replace hard-wired pass sequences with a small declarative graph — declare
named texture resources and passes (inputs/outputs + callback); `compile()` orders them
and creates one FBO per pass; `execute()` runs them. The `DeferredRenderer` post chain
(SSAO → lighting → SSR → TAA → bloom → god rays → composite) runs on it.

## Usage

```cpp
fuhgl::render::RenderGraph g;
g.addResource("hdr", w, h, fuhgl::gl::GL_RGBA16F);      // owned
g.addExternal("gPosition", &pipeline.texPosition(), w, h); // borrowed (G-buffer)

g.addPass("lighting", {"gPosition", "gNormal"}, {"hdr"},
          [](fuhgl::render::RenderGraph::PassContext& ctx) {
              ctx.bindInput(0, "gPosition");
              /* bind material, set uniforms, draw fullscreen triangle into ctx.framebuffer() */
          });
g.addPass("taa", {"hdr", "taa"}, {"taa"},               // feedback = read previous frame
          [](fuhgl::render::RenderGraph::PassContext& ctx) { /* ... */ },
          /*feedback=*/true);

if (!g.compile()) { /* cycle or unknown resource */ }
g.execute();          // every frame
g.setPassEnabled("taa", false);  // runtime toggle
g.resize(newW, newH);
```

## Semantics

- An edge A → B exists when B reads a resource A writes. A resource may have several
  writers (ping-pong): a reader depends on the **latest writer declared before it**.
- Forward references (reading a resource written only later) create edges too — so real
  cycles make `compile()` return `false`.
- `feedback = true` lets a pass read its own output (previous frame) — used by TAA.
- Passes with an empty output list don't get an FBO; their callback binds its own target
  (the composite pass renders into `pipeline.output()` this way).
- The pure resolver `render::resolvePassOrder()` is header-only and unit-tested without GL.

## Result

`gl_smoke_graph` builds a two-pass graph (fill red → scale into `b`), executes it and
verifies the readback pixels (R ≈ B ≈ 127, G = 0); a cyclic graph is rejected by
`compile()`. `DeferredRenderer::postGraph()` exposes the live post chain for inspection
and per-pass toggling.

## Notes

- Tests: `tests/test_graph.cpp` (unit: chain/diamond/cycle/feedback/ping-pong),
  `tests/gl_smoke_graph.cpp` (label `gl_context`).
