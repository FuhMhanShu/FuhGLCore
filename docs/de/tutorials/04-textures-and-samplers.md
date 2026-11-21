# 04 — Texturen (2D / 3D / Array / Cube) und Sampler-Objekte

> 🇬🇧 **English version:** [docs/en/tutorials/04-textures-and-samplers.md](../../en/tutorials/04-textures-and-samplers.md)

**Ziel:** Pixeldaten in beliebige Textur-Targets hochladen und das Filtern über ein separates
Sampler-Objekt steuern.

```cpp
fuhgl::Texture tex(fuhgl::Texture::Target::Tex2D);        // Tex3D, Tex2DArray, Cube, CubeArray,
                                                          // Tex2DMultisample(+Array)
tex.bind(fuhgl::gl::GL_TEXTURE0);                         // aktiviert die Einheit + bindet
tex.texStorage2D(6, fuhgl::gl::GL_RGBA8, 256, 256);        // unwandelbarer Speicher, Mip-Kette (GL 4.2)
tex.setImage2D(0, fuhgl::gl::GL_RGBA8, 256, 256, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, pixels);
tex.setSubImage2D(0, 0, 0, 64, 64, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, sub);
tex.generateMipmap();

// 3D / Array:
tex.setImage3D(0, fuhgl::gl::GL_RGBA8, 64, 64, 32, fuhgl::gl::GL_RGBA, fuhgl::gl::GL_UNSIGNED_BYTE, data3d);
// Cube-Seite:
tex.setImageCubeFace(fuhgl::Texture::CubeFace::PosX, 0, fuhgl::gl::GL_RGB8, 512, fuhgl::gl::GL_RGB, fuhgl::gl::GL_UNSIGNED_BYTE, face);

// Sampler-Objekt (GL 3.3): einmal setzen, pro Einheit binden
fuhgl::Sampler smp; smp.create();
smp.setParameteri(fuhgl::gl::GL_TEXTURE_MIN_FILTER, fuhgl::gl::GL_LINEAR_MIPMAP_LINEAR);
smp.setParameteri(fuhgl::gl::GL_TEXTURE_WRAP_S, fuhgl::gl::GL_REPEAT);
smp.bind(0);                                              // Einheit 0 nutzt jetzt diesen Sampler
```

Im Shader: `uniform sampler2D uTex;` / `sampler3D` / `sampler2DArray` / `samplerCube`
und `gl::activeTexture(GL_TEXTURE0)` + `setUniform("uTex", 0)`, um die Einheit zuzuweisen.

Für MSAA-Texturen `Target::Tex2DMultisample` + `storageMultisample2D(...)` verwenden und
über [05](05-framebuffers-and-msaa.md) auflösen.
