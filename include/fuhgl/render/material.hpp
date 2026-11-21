#pragma once

/// @file material.hpp
/// @brief Material: a shader program plus texture bindings, bound in one call.

#include "../gl/program.hpp"
#include "../gl/texture.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace fuhgl::render {

/// @brief Texture units used by the built-in pipelines (central constants — F26).
/// Units 0..2 stay free for user textures. Note: unit 4 carries the shadow map in the
/// lighting passes and the normal map in the deferred G-buffer pass; unit 5 carries
/// the parallax height map in the G-buffer pass and the SSAO map in the lighting pass
/// (the passes run at different times, so sharing a unit is safe).
inline constexpr int kTextureUnitAlbedo = 3;    ///< material albedo texture
inline constexpr int kTextureUnitShadowMap = 4; ///< shadow depth map (forward + lighting passes)
inline constexpr int kTextureUnitNormalMap = 4; ///< normal/bump map (deferred G-buffer pass)
inline constexpr int kTextureUnitHeightMap = 5; ///< parallax height map (deferred G-buffer pass)
inline constexpr int kTextureUnitSSAO = 5;      ///< SSAO map (deferred lighting pass)
inline constexpr int kTextureUnitGBufferPbr = 6; ///< metallic/roughness/ao G-buffer texture (deferred lighting pass, F28)
inline constexpr int kTextureUnitIrradiance = 7; ///< IBL irradiance cubemap (F29)
inline constexpr int kTextureUnitPrefilter = 8;  ///< IBL prefiltered env cubemap (F29)
inline constexpr int kTextureUnitBrdfLut = 9;    ///< IBL BRDF LUT (F29)
inline constexpr int kTextureUnitShadowMapArray = 10; ///< CSM depth array (F30; dummy array when off)

/// @brief Program + texture bindings. bind() makes the program current and binds
/// all textures to their units. Pipelines set the standard uniforms
/// ("uModel", "uViewProj", "uCamPos", …) themselves before drawing.
///
/// A Material can also be a *preset* (builtinShader = true, see materials.hpp):
/// then it carries only data (albedo/emissive/albedoTexture) and the pipelines
/// drive their built-in shaders with it via the uniforms uAlbedo / uEmissive /
/// uUseTex / uMatTex (albedo texture on unit kTextureUnitAlbedo).
class Material {
public:
    Material();
    ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&&) noexcept;
    Material& operator=(Material&&) noexcept;

    /// @brief Creates the underlying program object. Compile/link shaders, then done.
    /// Not needed for preset materials (builtinShader = true).
    bool create();

    Program& program() { return program_; }
    const Program& program() const { return program_; }

    /// @brief Binds a texture to a sampler unit (takes effect on bind()).
    void setTexture(gl::GLuint unit, const Texture& texture);
    void setTexture(gl::GLuint unit, gl::GLuint textureId);
    void setTexture(gl::GLuint unit, gl::GLenum target, gl::GLuint textureId);

    /// @brief Makes the program current and binds all textures.
    void bind() const;

    /// @brief Clears all texture bindings.
    void clearTextures();

    // --- preset fields (used when builtinShader = true) --------------------------
    math::Vec3 albedo{1.f, 1.f, 1.f};       ///< base color (multiplied with the texture if any)
    math::Vec3 emissive{0.f, 0.f, 0.f};     ///< added unlit on top of the lighting
    const Texture* albedoTexture = nullptr; ///< sampled on unit kTextureUnitAlbedo (3) when set
    const Texture* normalMap = nullptr;     ///< bump/normal map, unit kTextureUnitNormalMap (4, deferred G-buffer pass)
    const Texture* heightMap = nullptr;     ///< parallax height map, unit kTextureUnitHeightMap (5, deferred G-buffer pass)
    float parallaxScale = 0.03f;            ///< POM depth scale (world units at uv scale 1)
    int parallaxLayers = 12;                ///< POM linear search steps
    // --- PBR (F28): metallic-roughness model, sampled per pixel from the deferred
    // G-buffer (kTextureUnitGBufferPbr) or set as uniforms in the forward pass ---
    float metallic = 0.f;                   ///< 0 = dielectric, 1 = metal
    float roughness = 0.5f;                ///< 0 = smooth mirror, 1 = fully rough
    float ao = 1.f;                         ///< ambient occlusion multiplier
    bool builtinShader = false;             ///< true = pipeline built-in shader reads the fields above

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Program program_;
};

} // namespace fuhgl::render
