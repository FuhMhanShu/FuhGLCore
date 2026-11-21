# 04 — Textures (2D / 3D / array / cube) and sampler objects

**Goal:** upload pixel data to any texture target and control filtering via a
separate sampler object.

```cpp
fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);        // Tex3D, Tex2DArray, Cube, CubeArray,
                                                          // Tex2DMultisample(+Array)
tex.bind(fuhgl::gl::GL_TEXTURE0);                         // activates unit + binds
tex.texStorage2D(6, fuhgl::gl::GL_RGBA8, 256, 256);        // immutable storage, mip chain (GL 4.2)
tex.setImage2D(0, fuhgl::gl::GL_RGBA8, 256, 256, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, pixels);
tex.setSubImage2D(0, 0, 0, 64, 64, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, sub);
tex.generateMipmap();

// 3D / array:
tex.setImage3D(0, fuhgl::gl::GL_RGBA8, 64, 64, 32, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, data3d);
// cube face:
tex.setImageCubeFace(fuhgl::Texture::CubeFace::PosX, 0, fuhgl::gl::GL_RGB8, 512, fuhgl::gl::GL_RGB, fuhgl::gl::GL_UNSIGNED_BYTE, face);

// sampler object (GL 3.3): set once, bind per unit
fuhgl::Sampler smp; smp.create();
smp.setParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER, fuhgl::gl::GL_LINEAR_MIPMAP_LINEAR);
smp.setParameteri(fuhgl::gl::GL_TEXTURE_WRAP_S, fuhgl::gl::GL_REPEAT);
smp.bind(0);                                              // unit 0 now uses this sampler
```

In the shader: `uniform sampler2D uTex;` / `sampler3D` / `sampler2DArray` / `samplerCube`
and `gl::activeTexture(GL_TEXTURE0)` + `setUniform("uTex", 0)` to assign the unit.

For MSAA textures use `Target::Tex2DMultisample` + `storageMultisample2D(...)` and resolve
via [05](05-framebuffers-and-msaa.md).
