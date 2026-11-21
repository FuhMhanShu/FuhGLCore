# 38 — Bindless Textures (GL_ARB_bindless_texture)

> 🇬🇧 **English version:** [docs/en/tutorials/38-bindless-textures.md](../../en/tutorials/38-bindless-textures.md)

**Ziel:** Texturen im Shader ohne Texture-Unit-Binding sampeln — ein 64-Bit-Handle in
einem `uint64_t`-Uniform genügt. Das entfernt den Texture-Unit-Flaschenhals und erlaubt
Shadern Zugriff auf beliebige Textur-Arrays (z. B. eine bindless Material-Bibliothek).

![Bindless-Textur über 64-Bit-Handle gesampelt](../../images/tutorial-bindless.png)

## Verwendung

```cpp
fuhgl::Texture albedo(fuhgl::Texture::Target::Tex2D);
albedo.bind();
albedo.setImage2D(0, GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

fuhgl::gl::ext::BindlessTexture handle;
if (handle.attach(albedo)) {            // Handle erzeugen + resident machen
    prog.use();
    fuhgl::gl::ext::setUniformHandle(prog, "uTex", handle.handle());
    // ... zeichnen ...
}                                       // Destruktor: wieder non-resident
```

Fragment-Shader:

```glsl
#version 330 core
#extension GL_ARB_bindless_texture : require
uniform uint64_t uTex;
void main() { FragColor = texture(sampler2D(uTex), vUV); }
```

## Wie es funktioniert (F35)

`gl/ext/bindless.hpp` löst die `GL_ARB_bindless_texture`-Einstiegspunkte
(`glGetTextureHandleARB`, `glMakeTextureHandleResidentARB`,
`glMakeTextureHandleNonResidentARB`, `glIsTextureHandleResidentARB`,
`glUniformHandleui64ARB`, `glProgramUniformHandleui64ARB`) über `gl::procAddress()` auf:

- `ext::supported()` — Extension vorhanden und alle Zeiger aufgelöst (auf macOS false,
  dort gibt es kein Bindless; Code muss damit guarded werden),
- `ext::BindlessTexture` — RAII-Handle: `attach(tex)` erzeugt + resident,
  `detach()`/Destruktor macht non-resident, `handle()`/`isResident()` fragen ab,
- `ext::setUniformHandle(prog, name, handle)` — setzt das `uint64_t`-Uniform (über
  `glProgramUniformHandleui64ARB` falls vorhanden, sonst `glUniformHandleui64ARB`).

Das Textur-Objekt bleibt im Besitz des Aufrufers; der Handle ist nur gültig, solange die
Textur resident ist.

## Ergebnis

`gl_smoke_bindless` lädt eine 8×8-Textur bekannter Farbe, sampelt sie über einen
Bindless-Handle in einem Vollbild-Pass und prüft, dass das Zentrum-Pixel der Farbe
entspricht; außerdem wird geprüft, dass `detach()` den Handle non-resident macht. Skippt
mit Code 77, wenn die Extension fehlt (z. B. macOS).

## Hinweise

- Handles sind 64-Bit und opak; nie über die Resident-Lebensdauer hinaus speichern.
- Kombination mit F34 (GPU-Culling): Eine Instanz kann einen Textur-Handle tragen und ihn
  ohne jegliches Texture-Unit-Management sampeln.
- Test: `tests/gl_smoke_bindless.cpp` (Label `gl_context`, skippt ohne Extension).
