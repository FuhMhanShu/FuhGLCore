# 14 — Geräte-Infos & Fähigkeiten

> 🇬🇧 **English version:** [docs/en/tutorials/14-device-info-and-capabilities.md](../../en/tutorials/14-device-info-and-capabilities.md)

**Ziel:** auslesen, was der aktuelle OpenGL-Kontext / die GPU tatsächlich bietet —
Hersteller- & Renderer-Strings, GL-/GLSL-Version, Erweiterungsliste, Implementierungs-Limits
und Feature-Flags — mit einem Aufruf: `fuhgl::gl::queryDeviceInfo()`.

![Capability-Balken](../../images/tutorial-gpuinfo.png)

*(Screenshot headless in der CI gerendert: `examples/gpuinfo --screenshot` zeichnet die
wichtigsten Limits als Balken mit Text-Labels über `render::TextRenderer` — siehe
[Tutorial 12](12-headless-rendering-and-screenshots.md) und
[Tutorial 24](24-text-and-fps-overlay.md).)*

## Alles auf einmal abfragen

```cpp
fuhgl::Window window;
window.create("app", 800, 600);          // erzeugt einen Kontext und ruft gl::init() auf

fuhgl::gl::DeviceInfo info = fuhgl::gl::queryDeviceInfo();
std::printf("%s", info.report().c_str()); // menschenlesbarer Dump
```

`DeviceInfo` ist ein reiner Werttyp (kein PIMPL — enthält nur Strings/Ints):

| Feld | Bedeutung |
|---|---|
| `vendor`, `renderer`, `versionString`, `glslVersion` | rohe `glGetString`-Werte |
| `version` | geparste `gl::Version` (`atLeast(4,5)`, …); numerische Abfrage mit String-Fallback |
| `extensions` | komplette Liste (GL-4.3-Pfad `glGetStringi` mit Legacy-Fallback `GL_EXTENSIONS`) |
| `limits` | kuratierte `Limits`: max. Texturgrößen (2D/3D/Cube/Array-Layer), Renderbuffer- & Viewport-Größe, Vertex-Attribute, Uniform-Vektoren, Textur-Einheiten, MRT (`maxDrawBuffers`/`maxColorAttachments`), MSAA `maxSamples`, UBO/SSBO-Bindings **und** -Größen (64-Bit), Compute-Workgroup-Anzahl/-Größe/-Invocations, Elementanzahlen, Anisotropie |
| `features` | abgeleitete Flags: `coreProfile`, `dsa` (= `gl::hasDsa()`), `compute`, `debugOutput`, `anisotropicFiltering`, `shaderStorage` |

## Einzelne Bausteine

```cpp
info.hasExtension("GL_EXT_texture_filter_anisotropic"); // exakter Treffer in der Liste
fuhgl::gl::hasExtension("GL_KHR_debug");                 // kontextweiter Helfer
fuhgl::gl::extensions();                                 // std::vector<std::string>

if (info.features.dsa)      // gl::dsa()-Fast-Path nutzen (Tutorial 11)
if (info.features.compute)  // gl::dispatchCompute ist sicher (Tutorial 09)
if (info.limits.maxSamples >= 4)
    pipeline.create(w, h, /*msaaSamples=*/4);            // MSAA-G-Buffer (Tutorial 13)
```

Ohne aktuellen Kontext liefert `queryDeviceInfo()` ein leeres `DeviceInfo`
(Version `{0,0}`) — es wirft nie, wie der Rest der Bibliothek.

## Beispiel

`examples/gpuinfo` gibt den kompletten Report aus und rendert die Limits als Balken
(windowed, Esc beendet):

```
./build/examples/gpuinfo
./build/examples/gpuinfo --screenshot gpuinfo.png   # headless PNG (CI-Docs-Job)
```

Tests: `tests/test_info.cpp` (Wertsemantik, `hasExtension`, `report()` — kein GL nötig)
und die Device-Info-Assertions in `tests/gl_smoke.cpp` (headless gegen einen echten
Kontext in der CI).
