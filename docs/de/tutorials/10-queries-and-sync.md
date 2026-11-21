# 10 — Timer-Queries und Fence-Syncs

> 🇬🇧 **English version:** [docs/en/tutorials/10-queries-and-sync.md](../../en/tutorials/10-queries-and-sync.md)

**Ziel:** GPU-Zeit messen und CPU/GPU synchronisieren, ohne zu stallen.

```cpp
fuhgl::Query timer;                 // standardmäßig GL_TIME_ELAPSED
timer.begin();
/* Frame zeichnen */
timer.end();
if (timer.resultAvailable()) {
    fuhgl::gl::GLuint64 ns = timer.result();   // Nanosekunden auf der GPU
}

fuhgl::Query ts(fuhgl::gl::GL_TIMESTAMP);
ts.recordTimestamp();              // einzelner Zeitstempel

fuhgl::Fence fence;                 // wird bei der Konstruktion in den GL-Stream eingefügt
fuhgl::gl::GLenum st = fence.wait(1'000'000'000);  // wartet bis zu 1 s
if (st == fuhgl::gl::GL_ALREADY_SIGNALED || st == fuhgl::gl::GL_CONDITION_SATISFIED) { /* fertig */ }
// fence.wait(0) == nicht-blockierende Prüfung; GL_TIMEOUT_IGNORED wartet ewig
```

RAII: die GL-Objekte für Query/Sync werden automatisch gelöscht.
