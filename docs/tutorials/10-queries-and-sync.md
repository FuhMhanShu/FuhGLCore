# 10 — Timer queries and fence syncs

**Goal:** measure GPU time and synchronize CPU/GPU without stalling.

```cpp
fuhgl::Query timer;                 // GL_TIME_ELAPSED by default
timer.begin();
/* draw frame */
timer.end();
if (timer.resultAvailable()) {
    fuhgl::gl::GLuint64 ns = timer.result();   // nanoseconds on the GPU
}

fuhgl::Query ts(fuhgl::gl::GL_TIMESTAMP);
ts.recordTimestamp();              // single timestamp

fuhgl::Fence fence;                 // inserted into the GL stream at construction
fuhgl::gl::GLenum st = fence.wait(1'000'000'000);  // wait up to 1 s
if (st == fuhgl::gl::GL_ALREADY_SIGNALED || st == fuhgl::gl::GL_CONDITION_SATISFIED) { /* done */ }
// fence.wait(0) == non-blocking check; GL_TIMEOUT_IGNORED waits forever
```

RAII: the query/sync GL objects are deleted automatically.
