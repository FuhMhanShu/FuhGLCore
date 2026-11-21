#pragma once

/// @file materials.hpp
/// @brief Predefined materials: ready-made presets that drive the pipelines'
/// built-in shaders (no custom GLSL needed). A preset Material has
/// builtinShader = true and carries only data; the pipelines set the uniforms
/// uAlbedo / uEmissive / uUseTex / uMatTex (albedo texture on unit 3) per item.

#include "material.hpp"
#include "../gl/texture.hpp"
#include "../math.hpp"

namespace fuhgl::render::materials {

/// @brief Matte (diffuse) material with a flat albedo color.
inline Material matte(const math::Vec3& albedo = {0.8f, 0.8f, 0.8f}) {
    Material m;
    m.builtinShader = true;
    m.albedo = albedo;
    return m;
}

/// @brief Textured material: albedo texture (unit 3) modulated by a tint color.
inline Material textured(const Texture& albedoTex, const math::Vec3& tint = {1.f, 1.f, 1.f}) {
    Material m;
    m.builtinShader = true;
    m.albedo = tint;
    m.albedoTexture = &albedoTex;
    return m;
}

/// @brief Unlit/emissive material: the color is added on top of the lighting.
inline Material emissive(const math::Vec3& color, float strength = 1.f) {
    Material m;
    m.builtinShader = true;
    m.albedo = {0.f, 0.f, 0.f};
    m.emissive = color * strength;
    return m;
}

/// @brief Lit material with albedo color + texture + emissive term combined.
inline Material texturedEmissive(const Texture& albedoTex, const math::Vec3& tint,
                                 const math::Vec3& emissiveColor) {
    Material m;
    m.builtinShader = true;
    m.albedo = tint;
    m.albedoTexture = &albedoTex;
    m.emissive = emissiveColor;
    return m;
}

} // namespace fuhgl::render::materials
