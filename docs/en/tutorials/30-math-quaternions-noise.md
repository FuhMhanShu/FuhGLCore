# 30 — Quaternions and seeded noise in `fuhgl::math`

**Goal:** rotate things without Euler-angle gimbal lock, and generate deterministic
procedural values (terrain heights, placement jitter, texture seeds) — all header-only
value types, no GL context needed.

## Quaternions (`math::Quat`)

```cpp
#include <fuhgl/math.hpp>
using namespace fuhgl::math;

Quat q = fromAxisAngle(radians(90.f), {0.f, 1.f, 0.f}); // 90 deg around Y
Vec3 v = q * Vec3{1.f, 0.f, 0.f};                      // rotate a vector -> (0, 0, -1)

Mat4 m = toMat4(q);                    // same rotation as rotate(radians(90.f), {0,1,0})
Quat e = fromEuler(yaw, pitch, roll);  // yaw around Y, then pitch X, then roll Z
Quat l = lookRotation({0.f, 0.f, -1.f}, {0.f, 1.f, 0.f}); // identity (default forward)

Quat half = slerp(qA, qB, 0.5f);       // spherical interpolation (nlerp = cheap approx)
Quat back = inverse(q);                // undo: back * (q * v) == v
```

`q * v` is the rotation `q * v * q^-1` (Hamilton convention, right-handed) and is
**identical** to `rotate(angle, axis) * v` — the unit tests verify
`toMat4(fromAxisAngle(a, axis)) == rotate(a, axis)` element-wise. `q` and `-q` are the
same rotation (double cover). Also: `fromMat3`, `toAxisAngle`, `angle(a, b)`,
`dot/length/normalize/conjugate`.

## Seeded noise (`math::Noise`)

```cpp
Noise noise(42);                       // permutation table from seed — deterministic
float h  = noise.perlin2(x, y);        // improved Perlin 2D, approx. [-1, 1]
float h3 = noise.perlin3(x, y, z);     // 3D variant
float f  = noise.fbm2(x, y, 5);        // fractal Brownian motion, amplitude-normalized
float f3 = noise.fbm3(x, y, z, 4, 2.f, 0.5f); // octaves, lacunarity, gain

// one-shot helpers (construct a Noise internally):
float v = perlin2(x, y, /*seed=*/42);
```

Same seed → same values on every platform; integer lattice points evaluate to exactly 0.
Typical 2D statistics: mean ≈ 0, stddev ≈ 0.25, observed range ≈ ±0.8 (hard bound 2).

## Result

```
$ ./noise_demo
q * (1,0,0)      = (0.000000, 0.000000, -1.000000)   // 90 deg around Y
toMat4(q) == rotate(90 deg, Y) element-wise: true
perlin2(1.25, -3.5) seed 42 = -0.31207   (same on every run)
fbm2(3.3, 7.7, 4)  seed 42 =  0.18442
```

(The exact numbers are deterministic — the unit tests in `tests/test_noise.cpp`
check determinism, lattice zeros, range, mean and spread statistically.)

## Notes

- Both types are plain value types (no PIMPL, no GL handles) like the rest of
  `fuhgl::math`; `constexpr` where C++17 allows it.
- `math::Noise` is independent of `render::procedural`'s texture generators — use it
  for CPU-side heightmaps, scatter positions, animation jitter, etc.
- Tests: `tests/test_quat.cpp`, `tests/test_noise.cpp` (label `unit`, run headless).
