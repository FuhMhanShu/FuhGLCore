# 31 — PBR-Materialien (Metallic-Roughness, Cook-Torrance GGX)

> 🇬🇧 **English version:** [docs/en/tutorials/31-pbr-materials.md](../../en/tutorials/31-pbr-materials.md)

**Ziel:** Materialien mit einem physikalisch basierten Metallic-Roughness-Modell
(GGX-Specular + Fresnel) in beiden eingebauten Pipelines shaden — der moderne Default
in Unreal/Unity/Filament.

## Materialien

```cpp
#include <fuhgl/render.hpp>

fuhgl::render::Material gold    = fuhgl::render::materials::pbr({0.95f, 0.64f, 0.22f}, 1.0f, 0.25f);
fuhgl::render::Material plastic = fuhgl::render::materials::pbr({0.8f, 0.1f, 0.1f}, 0.0f, 0.5f);
fuhgl::render::Material roughMetal = fuhgl::render::materials::pbr({0.7f, 0.7f, 0.7f}, 1.0f, 0.9f);

// texturierte Variante (Albedo-Textur auf Unit 3, getintet):
fuhgl::render::Material floorTex = fuhgl::render::materials::pbrTextured(tex, {1,1,1}, 0.f, 0.8f);
```

`Material` hat drei neue Preset-Felder: `metallic` (0 = Dielektrikum, 1 = Metall),
`roughness` (0 = Spiegel, 1 = voll rau), `ao` (AO-Multiplikator). Die alten
`matte()/textured()/emissive()`-Presets funktionieren weiter — sie sind nur PBR mit
`metallic = 0`.

## Wie die Pipelines das nutzen

- **Forward**: die Felder sind Uniforms pro Item (`uMetallic/uRoughness/uAo`, gesetzt in
  `detail::bindItem`).
- **Deferred**: der G-Buffer hat ein 5. Attachment bekommen (`texPbr()`, RGBA8 =
  metallic/roughness/ao, Textur-Unit 6, wie die anderen MSAA-resolved); der
  Lighting-Pass liest es pro Pixel.

Das gemeinsame GLSL (`src/render/lighting_glsl.hpp`, `kPbrFuncs`) implementiert
`distributionGGX` / `geometrySmith` / `fresnelSchlick` und
`shade(albedo, metallic, roughness, ao, N, worldPos, V)` — GGX-Direktterm pro Licht
plus Ambient-Term (IBL falls gesetzt, siehe [Tutorial 32](32-ibl-environment.md)), mit
PCF-Schatten auf der Schatten-Lichtquelle wie bisher. Hinweis: der Diffuse-Term ist
`kD * albedo` ohne 1/π und damit konsistent zum IBL-Bake.

## Ergebnis

Eine Metallkugel (metallic 1, roughness 0.15) zeigt unter direktionalem Licht einen
scharfen, albedo-gefärbten Highlight; dieselbe Kugel mit roughness 1 verteilt ihn zu
einem weichen Schimmer; ein Dielektrikum (metallic 0) behält den klassischen
weißlichen Specular. Der Smoke-Test `gl_smoke_pbr` prüft alle drei Fälle per
Pixel-Readback und dass das 5. G-Buffer-Attachment wirklich `metallic = 1` (R = 255) in
der Box-Mitte speichert.

## Hinweise

- Eigene (non-preset) Materialien sind unberührt — sie laufen ihren eigenen Shader; die
  PBR-Uniforms werden dort ignoriert.
- Test: `gl_smoke_pbr.cpp` (Label `gl_context`, skip ohne GL 3.3).
