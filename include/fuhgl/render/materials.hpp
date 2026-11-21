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

/// @brief Bump/normal-mapped material: albedo texture + tangent-space normal map
/// (unit 4 in the deferred G-buffer pass; the normal perturbs the shading normal).
inline Material normalMapped(const Texture& albedoTex, const Texture& normalMap,
                             const math::Vec3& tint = {1.f, 1.f, 1.f}) {
    Material m;
    m.builtinShader = true;
    m.albedo = tint;
    m.albedoTexture = &albedoTex;
    m.normalMap = &normalMap;
    return m;
}

/// @brief Parallax-occlusion-mapped material: albedo texture + height map (unit 5),
/// UVs are displaced along the view ray in the deferred G-buffer pass.
inline Material parallax(const Texture& albedoTex, const Texture& heightMap,
                         float scale = 0.03f, int layers = 12,
                         const math::Vec3& tint = {1.f, 1.f, 1.f}) {
    Material m;
    m.builtinShader = true;
    m.albedo = tint;
    m.albedoTexture = &albedoTex;
    m.heightMap = &heightMap;
    m.parallaxScale = scale;
    m.parallaxLayers = layers;
    return m;
}

} // namespace fuhgl::render::materials
