#pragma once

/// @file post_chain.hpp
/// @brief PostChain (F42): a reusable post-processing chain on a render::RenderGraph for
/// ANY HDR input (used by ForwardRenderer for its post FX; DeferredRenderer has its own
/// G-buffer-coupled chain in deferred_renderer.cpp). Passes: TAA (F32) -> DoF (F36) ->
/// volumetric fog (F37) -> bloom bright+blur -> god rays -> ACES composite into an LDR
/// target. DoF/vol fog reconstruct world positions from the DEPTH texture (no G-buffer
/// needed); SSAO/SSR stay deferred-only. All effects toggleable (F40) and capability-
/// gated (F39) — unsupported passes are skipped automatically.

#include "effects.hpp"
#include "graph.hpp"
#include "material.hpp"
#include "scene.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/texture.hpp"
#include "../gl/vao.hpp"

#include <functional>
#include <memory>
#include <string>

namespace fuhgl::render {

/// @brief Screen-space UV of the sun (direction `sunDir` at infinity) for god rays
/// (defined in deferred_renderer.cpp; declared here for PostChain's rays pass).
math::Vec2 sunScreenUV(const Camera& camera, const math::Vec3& sunDir, int width, int height);

/// @brief Post-processing chain over an HDR texture. Usage:
/// `create(w, h)`, per frame `execute(scene, hdrTexture, depthTexture, ldrTarget)`.
class PostChain {
public:
    PostChain();
    ~PostChain();

    PostChain(const PostChain&) = delete;
    PostChain& operator=(const PostChain&) = delete;
    PostChain(PostChain&&) noexcept;
    PostChain& operator=(PostChain&&) noexcept;

    /// @brief Builds the graph + materials (needs a current GL context).
    bool create(int w, int h);
    void resize(int w, int h);
    int width() const { return width_; }
    int height() const { return height_; }

    /// @brief Runs the chain: `hdr` = HDR RGBA16F input, `depth` = its depth texture
    /// (used by DoF/vol fog; pass any 1x1 depth texture when those are off), `out` = LDR target.
    void execute(const SceneView& scene, const Texture& hdr, const Texture& depth, Framebuffer& out);

    RenderGraph& graph() { return post_; }
    /// @brief Graph resource texture (e.g. "volfog", "taa") — for debug/introspection.
    const Texture* resource(std::string_view name) const { return post_.texture(name); }

    // --- config (same names as DeferredRenderer where applicable) ----------------------
    bool bloomEnabled = true;
    float bloomStrength = 0.7f;
    float bloomThreshold = 1.0f;
    int bloomBlurPasses = 2;
    bool godRaysEnabled = true;
    float rayDensity = 0.92f, rayDecay = 0.95f, rayWeight = 0.35f;
    float exposure = 1.0f;
    bool taaEnabled = false;      ///< F32 temporal AA (needs jitter on the producing pipeline)
    float taaBlend = 0.1f;
    bool dofEnabled = false;      ///< F36 depth of field (CoC from the depth texture)
    float dofFocusDist = 8.f;
    float dofFocusRange = 6.f;
    float dofStrength = 12.f;
    int dofTaps = 16;
    bool volFogEnabled = false;   ///< F37 volumetric fog (raymarched from the depth texture)
    float volFogDensity = 0.02f;
    math::Vec3 volFogColor{0.75f, 0.80f, 0.88f};
    float volFogAnisotropy = 0.3f;
    float volFogHeight = 0.f;
    float volFogFalloff = 0.15f;
    bool volFogShadows = false;   ///< attenuate in-scattering via SceneView::shadowMap/cascades
    int volFogSteps = 24;
    float volFogMaxDist = 150.f;

    // --- F39/F40: capability gating + uniform toggle API --------------------------------
    bool effectSupported(Effect e) const;
    bool effectEnabled(Effect e) const;
    void setEffectEnabled(Effect e, bool on);
    void setAllEffects(bool on);

private:
    bool createGraph(int w, int h);
    void captureCaps();
    void gate(Effect e, bool& flag);
    void applyCapabilities();

    void drawFs(Material& mat, const std::function<void(Program&)>& setUniforms = nullptr);
    void drawCopy(RenderGraph::PassContext& ctx, std::string_view in);
    void drawTAA(RenderGraph::PassContext& ctx);
    void drawDoF(RenderGraph::PassContext& ctx);
    void drawVolFog(RenderGraph::PassContext& ctx);
    void drawBlur(RenderGraph::PassContext& ctx, std::string_view in, const math::Vec2& dir);
    void drawBloomBright(RenderGraph::PassContext& ctx);
    void drawRays(RenderGraph::PassContext& ctx);
    void drawComposite(RenderGraph::PassContext& ctx, std::string_view bloomRes);

    RenderGraph post_;
    Material copyMat_, blurMat_, brightMat_, raysMat_, compositeMat_, taaMat_, dofMat_, volfogMat_;
    VertexArray fsVao_;
    // externals point at these dummies; the real per-frame textures are bound manually in
    // the pass lambdas from curHdr_/curDepth_
    Texture dummyHdr_{Texture::Target::Tex2D};
    Texture dummyDepth_{Texture::Target::Tex2D};
    Texture dummyShadowTex_{Texture::Target::Tex2D};
    Texture dummyShadowArrayTex_{Texture::Target::Tex2DArray};
    int width_ = 0, height_ = 0;
    unsigned frameIndex_ = 0;
    const SceneView* curScene_ = nullptr;
    const Texture* curHdr_ = nullptr;
    const Texture* curDepth_ = nullptr;
    Framebuffer* out_ = nullptr;
    std::string bloomLast_;
    int glMajor_ = 0, glMinor_ = 0;
    bool hasBindless_ = false, hasCompute_ = false;
    unsigned warnedMask_ = 0;
};

} // namespace fuhgl::render
