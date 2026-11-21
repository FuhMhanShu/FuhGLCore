#pragma once

// Internal helpers shared by the built-in pipelines (forward + deferred).

#include "fuhgl/render/material.hpp"
#include "fuhgl/render/scene.hpp"
#include "fuhgl/gl/loader.hpp"
#include "fuhgl/gl/state.hpp"

namespace fuhgl::render::detail {

/// @brief Binds the material for a draw item and sets the per-item uniforms
/// (uModel/uViewProj + material fields). Preset materials (builtinShader = true)
/// drive the pipeline's default program via uAlbedo/uEmissive/uUseTex/uMatTex
/// (albedo texture on kTextureUnitAlbedo); custom materials get their own program bound.
inline void bindItem(const Material& defaultMat, const DrawItem& item,
                     const math::Mat4& model, const math::Mat4& viewProj) {
    const Material* preset = (item.material && item.material->builtinShader) ? item.material : nullptr;
    const Material& mat = preset ? defaultMat : (item.material ? *item.material : defaultMat);
    mat.bind();
    Program& prog = const_cast<Program&>(mat.program());
    prog.setUniform("uModel", model);
    prog.setUniform("uViewProj", viewProj);
    prog.setUniform("uInstanced", (item.mesh && item.mesh->instanceCount() > 0) ? 1 : 0); // F24
    if (preset) {
        prog.setUniform("uAlbedo", preset->albedo);
        prog.setUniform("uEmissive", preset->emissive);
        prog.setUniform("uUseTex", preset->albedoTexture ? 1 : 0);
        if (preset->albedoTexture) {
            gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitAlbedo);
            gl::functions().glBindTexture(static_cast<gl::GLenum>(preset->albedoTexture->target()),
                                          preset->albedoTexture->id());
            prog.setUniform("uMatTex", kTextureUnitAlbedo);
        }
        // bump / parallax maps (used by the deferred G-buffer pass; harmless elsewhere)
        prog.setUniform("uUseNormalMap", preset->normalMap ? 1 : 0);
        prog.setUniform("uUseHeightMap", preset->heightMap ? 1 : 0);
        if (preset->normalMap) {
            gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitNormalMap);
            gl::functions().glBindTexture(static_cast<gl::GLenum>(preset->normalMap->target()),
                                          preset->normalMap->id());
            prog.setUniform("uNormalTex", kTextureUnitNormalMap);
        }
        if (preset->heightMap) {
            gl::activeTexture(gl::GL_TEXTURE0 + kTextureUnitHeightMap);
            gl::functions().glBindTexture(static_cast<gl::GLenum>(preset->heightMap->target()),
                                          preset->heightMap->id());
            prog.setUniform("uHeightTex", kTextureUnitHeightMap);
        }
        prog.setUniform("uParallaxScale", preset->parallaxScale);
        prog.setUniform("uParallaxLayers", preset->parallaxLayers);
    } else {
        prog.setUniform("uAlbedo", math::Vec3{1.f, 1.f, 1.f});
        prog.setUniform("uEmissive", math::Vec3{0.f, 0.f, 0.f});
        prog.setUniform("uUseTex", 0);
    }
}

} // namespace fuhgl::render::detail
