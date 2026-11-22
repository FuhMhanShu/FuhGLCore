#pragma once

/// @file deferred_renderer.hpp
/// @brief DeferredRenderer (F17): the default ready-to-use deferred renderer —
/// G-buffer pass + SSAO + HDR lighting (lights, PCF/CSM shadows, IBL, fog, sky sun
/// disk), then a post-processing chain that runs through a render::RenderGraph (F31):
/// optional SSR (F33) and TAA (F32, subpixel jitter + history accumulation) on the HDR
/// image, then bloom (bright-pass + separable blur), crepuscular "god" rays from the
/// sun's screen position, and ACES tone mapping + gamma into the LDR output.
/// Parallax/bump mapping live in the G-buffer pass (material maps).

#include "deferred.hpp"
#include "graph.hpp"
#include "light.hpp"
#include "shadow.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/texture.hpp"
#include "../gl/ubo.hpp"
#include "../gl/vao.hpp"

#include <functional>
#include <memory>

namespace fuhgl::render {

/// @brief Screen-space UV of the sun (direction `sunDir` at infinity) for god rays.
/// Returns {-1,-1} when the sun is behind the camera.
math::Vec2 sunScreenUV(const Camera& camera, const math::Vec3& sunDir, int width, int height);

/// @brief Default deferred renderer with the full post-processing chain.
/// `render(scene)` draws everything into `output()` (RGBA8 LDR after tone mapping).
class DeferredRenderer {
public:
    DeferredRenderer();
    ~DeferredRenderer();

    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;
    DeferredRenderer(DeferredRenderer&&) noexcept;
    DeferredRenderer& operator=(DeferredRenderer&&) noexcept;

    /// @brief Creates the G-buffer + post chain. msaaSamples > 0 enables MSAA G-buffer.
    bool create(int width, int height, int msaaSamples = 0);
    void resize(int width, int height);

    /// @brief Full frame: shadow depth pass (if enabled) -> G-buffer -> post graph
    /// (SSAO -> lighting -> [SSR] -> [TAA] -> [DoF] -> bloom -> god rays -> composite into output()).
    void render(const SceneView& scene);

    Framebuffer& output() { return pipeline_.output(); }
    DeferredPipeline& pipeline() { return pipeline_; }
    /// @brief The post-processing render graph (F31) — passes can be toggled via setPassEnabled().
    RenderGraph& postGraph() { return post_; }
    int width() const { return width_; }
    int height() const { return height_; }
    /// @brief The HDR scene texture before post FX (graph resource "hdr" — debug/introspection).
    const Texture& hdrTexture() const;

    // --- post-processing config ------------------------------------------------------
    bool ssaoEnabled = true;
    float ssaoRadius = 0.6f;
    float ssaoStrength = 1.2f;
    int ssaoSamples = 32;
    float bloomStrength = 0.7f;
    float bloomThreshold = 1.0f;
    int bloomBlurPasses = 2;
    bool godRaysEnabled = true;
    float rayDensity = 0.92f, rayDecay = 0.95f, rayWeight = 0.35f;
    float exposure = 1.0f;
    // --- TAA (F32): temporal anti-aliasing with Halton subpixel jitter ------------------
    bool taaEnabled = false;
    float taaBlend = 0.1f;          ///< steady-state history weight (first frame = 1.0)
    // --- SSR (F33): screen-space reflections on the HDR image ---------------------------
    bool ssrEnabled = false;
    float ssrStrength = 0.6f;
    int ssrMaxSteps = 32;
    float ssrThickness = 0.3f;
    // --- DoF (F36): depth of field, bokeh gather pass after TAA --------------------------
    bool dofEnabled = false;
    float dofFocusDist = 8.f;   ///< world-space distance of the focus plane
    float dofFocusRange = 6.f;  ///< distance over which the CoC grows to dofStrength
    float dofStrength = 12.f;   ///< max circle of confusion in pixels
    int dofTaps = 16;           ///< gather taps (golden-angle spiral)

    // --- shadows (same model as ForwardRenderer) ---------------------------------------
    void enableShadows(int mapSize = 2048);
    void disableShadows() { shadows_ = false; }
    bool shadowsEnabled() const { return shadows_; }
    ShadowMap* shadowMap() { return shadows_ ? &shadowMap_ : nullptr; }
    void setShadowSceneBounds(const math::Vec3& center, float radius) { shadowCenter_ = center; shadowRadius_ = radius; }
    void setShadowStrength(float s) { shadowStrength_ = s; }
    /// @brief Camera depth range the cascades cover (F30); default 0.5 .. 200.
    void setShadowZRange(float zn, float zf) { shadowZNear_ = zn; shadowZFar_ = zf; }
    /// @brief CSM instead of the single ShadowMap (F30): one depth-array layer per cascade.
    void enableCsmShadows(int mapSize = 2048, int cascades = 4);
    void disableCsmShadows() { csmEnabled_ = false; }
    bool csmShadowsEnabled() const { return csmEnabled_; }
    CascadeShadowMap* cascadeShadowMap() { return csmEnabled_ ? &csm_ : nullptr; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    bool createPost(int w, int h);
    void drawFs(Material& mat, const std::function<void(Program&)>& setUniforms = nullptr);
    void drawSSAO(RenderGraph::PassContext& ctx);
    void drawBlur(RenderGraph::PassContext& ctx, std::string_view in, const math::Vec2& dir);
    void drawLighting(RenderGraph::PassContext& ctx);
    void drawSSR(RenderGraph::PassContext& ctx);
    void drawTAA(RenderGraph::PassContext& ctx);
    void drawDoF(RenderGraph::PassContext& ctx);
    void drawCopy(RenderGraph::PassContext& ctx, std::string_view in);
    void drawBloomBright(RenderGraph::PassContext& ctx);
    void drawRays(RenderGraph::PassContext& ctx);
    void drawComposite(RenderGraph::PassContext& ctx, std::string_view bloomRes);

    DeferredPipeline pipeline_;
    RenderGraph post_;
    ShadowMap shadowMap_;
    CascadeShadowMap csm_;
    int width_ = 0, height_ = 0, msaa_ = 0;
    const SceneView* curScene_ = nullptr;
    unsigned frameIndex_ = 0;
    std::string bloomLast_; ///< bloom resource the composite pass reads

    bool shadows_ = false;
    bool csmEnabled_ = false;
    float shadowZNear_ = 0.5f, shadowZFar_ = 200.f;
    int shadowSize_ = 2048;
    math::Vec3 shadowCenter_{0.f, 0.f, 0.f};
    float shadowRadius_ = 10.f;
    float shadowStrength_ = 1.f;

    Material ssaoMat_, blurMat_, brightMat_, raysMat_, compositeMat_, taaMat_, ssrMat_, dofMat_, copyMat_;
    VertexArray fsVao_;
};

} // namespace fuhgl::render
