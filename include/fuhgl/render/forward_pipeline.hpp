#pragma once

/// @file forward_pipeline.hpp
/// @brief ForwardPipeline: the concrete low-level forward pipeline (one pass,
/// per-fragment lighting). For a ready-to-use one-call renderer with shadows and an
/// owned target, use render::ForwardRenderer (forward_renderer.hpp) instead.

#include "pipeline.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/ubo.hpp"

namespace fuhgl::render {

/// @brief Low-level forward pipeline: one pass, per-fragment lighting, straight into
/// the target (own FBO when set via setTarget(), otherwise the currently bound FB).
/// Role vs. render::ForwardRenderer: a *Pipeline* is a building block you drive
/// yourself (bind a target, call render(scene) per frame); a *Renderer* wraps a
/// pipeline with an owned target, automatic shadow pass and post-processing, so one
/// call per frame is enough.
class ForwardPipeline : public RenderPipeline {
public:
    ForwardPipeline();
    ~ForwardPipeline() override;

    ForwardPipeline(const ForwardPipeline&) = delete;
    ForwardPipeline& operator=(const ForwardPipeline&) = delete;
    ForwardPipeline(ForwardPipeline&&) noexcept;
    ForwardPipeline& operator=(ForwardPipeline&&) noexcept;

    /// @brief Builds the default forward material (shaders compiled internally).
    /// @param width height accepted for interface symmetry with DeferredPipeline::create;
    /// the forward pipeline renders into the bound framebuffer or setTarget() FBO, so
    /// the size is unused unless an internal target is allocated.
    bool create(int width = 0, int height = 0);

    /// @brief Optional offscreen target; nullptr (default) renders to the bound FB.
    void setTarget(Framebuffer* fbo) { target_ = fbo; }
    /// @brief If a target is set: resolve/blit it to the default framebuffer.
    void blitToScreen();

    /// @brief Default material used for items with material == nullptr.
    /// Custom materials must use the standard uniform names (uModel, uViewProj, uCamPos).
    Material& defaultMaterial() { return defaultMaterial_; }

    void render(const SceneView& scene) override;

private:
    // Plain handle-owning class (like gl::Buffer/VertexArray): no PIMPL needed, the
    // members below are the state.
    Framebuffer* target_ = nullptr;
    Material defaultMaterial_;
    UniformBuffer lightsUbo_;
    Texture dummyShadowTex_{Texture::Target::Tex2D}; ///< 1x1 depth bound when no shadow map is set
    Texture dummyShadowArrayTex_{Texture::Target::Tex2DArray}; ///< 1x1x1 depth array bound when CSM is off (F30)
};

} // namespace fuhgl::render
