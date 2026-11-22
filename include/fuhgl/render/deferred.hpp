#pragma once

/// @file deferred.hpp
/// @brief Deferred renderer: G-buffer pass (albedo/normal/position via MRT) +
/// fullscreen lighting pass. Optional MSAA on the G-buffer with resolve.

#include "pipeline.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/renderbuffer.hpp"
#include "../gl/texture.hpp"
#include "../gl/ubo.hpp"

#include <memory>

namespace fuhgl::render {

/// @brief Deferred pipeline (low-level building block). Always renders into its own
/// output framebuffer; for the ready-to-use one-call renderer with SSAO/bloom/god-rays
/// see render::DeferredRenderer (deferred_renderer.hpp).
/// Role vs. render::DeferredRenderer: the *Pipeline* is driven manually (render(scene),
/// G-buffer accessors); the *Renderer* adds shadows + the full post chain per call.
/// blit it to the screen with output() + Framebuffer::blitTo, or read it headless.
class DeferredPipeline : public RenderPipeline {
public:
    DeferredPipeline();
    ~DeferredPipeline() override;

    DeferredPipeline(const DeferredPipeline&) = delete;
    DeferredPipeline& operator=(const DeferredPipeline&) = delete;
    DeferredPipeline(DeferredPipeline&&) noexcept;
    DeferredPipeline& operator=(DeferredPipeline&&) noexcept;

    /// @brief Creates G-buffer (albedo RGBA8, normal RGBA8, position RGBA16F + depth)
    /// and the lighting output target. msaaSamples > 0 enables MSAA G-buffer + resolve.
    bool create(int width, int height, int msaaSamples = 0);
    void resize(int width, int height);

    void render(const SceneView& scene) override;
    /// @brief Pass 1 only: G-buffer (+ MSAA resolve). Needed when post-processing
    /// (SSAO/HDR/bloom) sits between geometry and lighting — see DeferredRenderer.
    void renderGBuffer(const SceneView& scene);
    /// @brief Pass 2 only: fullscreen lighting into `target` (default: output()).
    void renderLighting(const SceneView& scene, Framebuffer* target = nullptr);

    /// @brief Optional SSAO map sampled on kTextureUnitSSAO in the lighting pass (F17).
    const Texture* aoTexture = nullptr;
    float aoStrength = 1.5f;

    Framebuffer& output() { return output_; }
    int width() const { return width_; }
    int height() const { return height_; }
    /// @brief G-buffer textures as the lighting pass samples them (resolved when MSAA).
    const Texture& texAlbedo() const { return msaa_ > 0 ? texResolveAlbedo_ : texAlbedo_; }
    const Texture& texNormal() const { return msaa_ > 0 ? texResolveNormal_ : texNormal_; }
    const Texture& texPosition() const { return msaa_ > 0 ? texResolvePosition_ : texPosition_; }
    const Texture& texEmissive() const { return msaa_ > 0 ? texResolveEmissive_ : texEmissive_; }
    /// @brief PBR G-buffer texture: R = metallic, G = roughness, B = ao (F28).
    const Texture& texPbr() const { return msaa_ > 0 ? texResolvePbr_ : texPbr_; }
    /// @brief Default G-buffer material for items with material == nullptr.
    Material& defaultMaterial() { return defaultMaterial_; }
    /// @brief Fullscreen lighting-pass material (debug/introspection).
    Material& lightingMaterial() { return lightingMaterial_; }
    /// @brief Light UBO (debug/introspection).
    const UniformBuffer& lightsUbo() const { return lightsUbo_; }
    /// @brief F46: when contact shadows need a sampleable G-buffer depth, call this once
    /// (non-MSAA only) to attach a depth *texture* instead of the renderbuffer.
    /// @return true when the G-buffer depth is sampleable afterwards.
    bool ensureSampleableDepth();
    /// @brief The sampleable depth texture (nullptr until ensureSampleableDepth() succeeded).
    const Texture* depthTexture() const { return depthTex_.id() ? &depthTex_ : nullptr; }

    /// @brief Sky colors used for background pixels (position alpha < 0.5) in the lighting pass.
    math::Vec3 skyTop{0.25f, 0.45f, 0.9f};
    math::Vec3 skyBottom{0.85f, 0.88f, 0.95f};

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int width_ = 0, height_ = 0, msaa_ = 0;
    Framebuffer gbuffer_;
    Framebuffer output_;
    Framebuffer resolve_; // only when msaa_ > 0
    Texture texAlbedo_{Texture::Target::Tex2D};
    Texture texNormal_{Texture::Target::Tex2D};
    Texture texPosition_{Texture::Target::Tex2D};
    Texture texEmissive_{Texture::Target::Tex2D};
    Texture texPbr_{Texture::Target::Tex2D};           ///< F28: metallic/roughness/ao
    Texture texResolvePbr_{Texture::Target::Tex2D};
    Texture texResolveAlbedo_{Texture::Target::Tex2D};
    Texture texResolveNormal_{Texture::Target::Tex2D};
    Texture texResolveEmissive_{Texture::Target::Tex2D};
    Texture texResolvePosition_{Texture::Target::Tex2D};
    Texture texOutput_{Texture::Target::Tex2D};
    Texture dummyShadowTex_{Texture::Target::Tex2D}; ///< 1x1 depth bound when no shadow map is set
    Texture dummyShadowArrayTex_{Texture::Target::Tex2DArray}; ///< 1x1x1 depth array bound when CSM is off (F30)
    Texture depthTex_{Texture::Target::Tex2D}; ///< F46: sampleable depth (ensureSampleableDepth), contact shadows
    Texture dummyMomentsTex_{Texture::Target::Tex2D};          ///< F46: 1x1 RGBA32F VSM dummy
    Texture dummyMomentsArrayTex_{Texture::Target::Tex2DArray}; ///< F46: 1x1x1 RGBA32F dummy
    Texture dummyCubeTex_{Texture::Target::Cube};               ///< F46: 1x1 cube depth dummy
    Texture dummyCubeMomentsTex_{Texture::Target::Cube};        ///< F46: 1x1 cube moments dummy
    Texture dummySceneDepthTex_{Texture::Target::Tex2D};        ///< F46: 1x1 scene depth dummy
    Renderbuffer depth_{};
    Renderbuffer depthMsaa_{};
    Material defaultMaterial_; // G-buffer program
    Material lightingMaterial_; // fullscreen lighting program
    VertexArray fullscreenVao_;
    UniformBuffer lightsUbo_;
};

} // namespace fuhgl::render
