# FuhGLCore

Another lightweight OpenGL wrapper — with **zero third-party dependencies**.

Modern C++17, static library, PIMPL idiom, English-documented, performance oriented.

## Features

- **Runtime GL loader** (`fuhgl::gl::init()`): loads OpenGL core functions 1.0–4.6
  by name at runtime — no GLAD/GLEW, no Khronos headers, no link dependency
  (Linux: `dlopen("libGL.so.1")`, Windows: `opengl32.dll`, macOS: OpenGL framework).
- **RAII wrappers** (PIMPL, movable, non-copyable): `Shader`, `Program`, `Buffer`,
  `VertexArray`, `Texture`, `Framebuffer`, `Renderbuffer` + thin state helpers.
- **Native window + GL context** (`fuhgl::Window`): Win32/WGL, X11/GLX, macOS Cocoa —
  no GLFW/SDL needed. Creates a 3.3 core-profile context (configurable) and calls
  `gl::init()` for you.
- **Math module** (`fuhgl::math`, header-only): `Vec2/3/4`, `Mat3/Mat4` column-major
  (directly compatible with `glUniformMatrix*fv`), transforms (`perspective`, `ortho`,
  `lookAt`, `translate/rotate/scale`).
- **Camera controllers** (`fuhgl`): `FirstPersonCamera`, `FreeCamera` (6-DOF),
  `IsometricCamera` — wired to `Window` events via `attach(window)`.

Baseline: **OpenGL 3.3 core** (up to 4.6 loaded opportunistically). No exceptions in
the API — failures return `bool`/info-log strings; debug builds assert and forward
GL debug output to a user callback.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix <prefix>   # optional: provides find_package(FuhGLCore)
```

Options: `-DFUHGL_BUILD_EXAMPLES=ON` (default), `-DFUHGL_BUILD_TESTS=ON` (default).

Requires only a C++17 compiler and CMake ≥ 3.20 — no OpenGL dev packages needed
at build time (functions are loaded at runtime).

## Usage

```cpp
#include <fuhgl/fuhgl.hpp>

int main() {
    fuhgl::Window window;
    if (!window.create("Triangle", 800, 600)) return 1;   // creates 3.3 core context + gl::init()

    fuhgl::Shader vs(fuhgl::Shader::Type::Vertex), fs(fuhgl::Shader::Type::Fragment);
    vs.compile(vertexSrc); fs.compile(fragmentSrc);
    fuhgl::Program prog; prog.attach(vs); prog.attach(fs); prog.link();

    fuhgl::FirstPersonCamera cam; cam.attach(window);

    while (!window.shouldClose()) {
        window.pollEvents();
        cam.update(1.0f / 60.0f);
        fuhgl::gl::clearColor(0.1f, 0.1f, 0.12f, 1.f);
        fuhgl::gl::clear(fuhgl::gl::ClearBit::Color | fuhgl::gl::ClearBit::Depth);
        prog.use();
        fuhgl::gl::setUniform(prog.uniformLocation("uViewProj"), cam.projection(800.f/600.f) * cam.view());
        fuhgl::gl::drawArrays(fuhgl::gl::Primitive::Triangles, 0, 3);
        window.swapBuffers();
    }
}
```

See `examples/triangle` for a complete program.

## Tests

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

Math, camera and loader-table tests run headless (no GL context required).

## License

MIT — see [LICENSE](LICENSE).
