#pragma once

/// @file deferred_renderer.hpp
/// @brief DeferredRenderer (F17): the default ready-to-use deferred renderer —
/// G-buffer pass + SSAO + HDR lighting (lights, PCF/CSM shadows, IBL, fog, sky sun
/// disk), then a post-processing chain that runs through a render::RenderGraph (F31):
/// optional SSR (F33) and TAA (F32, subpixel jitter + history accumulation) on the HDR
/// image, then bloom (bright-pass + separable blur), crepuscular "god" rays from the
/// sun's screen position, and ACES tone mapping + gamma into the LDR output.
/// Parallax/bump mapping live in the G-buffer pass (material maps).
/// Optional volumetric fog (F37) raymarches the world-space view ray per pixel after DoF.

#include "deferred.hpp"
#include "effects.hpp"
#include "graph.hpp"
#include "light.hpp"
#include "shadow.hpp"
#include "shadow_kit.hpp"

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
    /// (SSAO -> lighting -> [SSR] -> [TAA] -> [DoF] -> [volumetric fog] -> bloom -> god rays
    /// -> composite into output()).
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
    // --- volumetric fog (F37): raymarched single scattering along the view ray -----------
    bool volFogEnabled = false;
    float volFogDensity = 0.02f;              ///< extinction coefficient (1 / world unit)
    math::Vec3 volFogColor{0.75f, 0.80f, 0.88f}; ///< scattering albedo tint
    float volFogAnisotropy = 0.3f;            ///< Henyey-Greenstein g (forward scattering)
    float volFogHeight = 0.f;                 ///< if > 0: exponential height fog from this height
    float volFogFalloff = 0.15f;              ///< height fog falloff (exp(-(y-h) * falloff))
    bool volFogShadows = false;               ///< attenuate in-scattering by the shadow map/CSM
    int volFogSteps = 24;                     ///< raymarch steps per pixel
    float volFogMaxDist = 150.f;              ///< clamp of the marched ray length
    // --- LOD (F41): DrawItem::lod level selection by camera distance ----------------------
    bool lodEnabled = true;                   ///< false = always LOD level 0
    /// @brief LOD level chosen for the last drawn item that had a LodMesh (-1 = none). Tests/introspection.
    int lastLodLevel() const { return lastLodLevel_; }

    // --- effects: uniform toggle API + capability gating (F39/F40) ---------------------
    /// @brief Enables/disables one effect by enum. Unsupported effects are refused with a
    /// warning (they stay off — graceful degradation on old GPUs).
    void setEffectEnabled(Effect e, bool on);
    /// @brief True when the effect is both requested and supported by the current GPU.
    bool effectEnabled(Effect e) const;
    /// @brief Enables/disables all renderer-owned effects at once (still capability-gated).
    void setAllEffects(bool on);
    /// @brief True when the current GPU can run this effect at all (pure mapping, F39).
    bool effectSupported(Effect e) const {
        return render::effectSupported(e, glMajor_, glMinor_, hasBindless_, hasCompute_);
    }

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

    // --- F46: advanced shadow sampling on top of the shadow kit (render/shadow_kit.hpp) ---
    /// @brief Sampling filter of the shadow light: poisson PCF / PCSS / VSM (VSM renders
    /// moments into an extra RGBA32F attachment). Default Pcf3x3 = legacy behavior.
    ShadowFilter shadowFilter = ShadowFilter::Pcf3x3;
    void setShadowFilter(ShadowFilter f) { shadowFilter = f; syncShadowTargets(); }
    bool shadowSnapToTexels = false;  ///< snap light VPs to the shadow-map texel grid (less swimming)
    float shadowCascadeBlend = 0.f;   ///< CSM cross-fade fraction of a cascade span (0 = hard switch)
    float shadowLightSize = 1.f;      ///< PCSS: world-space light size (penumbra width)
    float shadowMinVariance = 5e-4f;  ///< VSM: min-variance clamp against light bleeding
    /// @brief Cube shadow map for one point light (F46): `lightIndex` into SceneView::lights.
    void enablePointShadow(int lightIndex, int mapSize = 1024);
    void disablePointShadow() { pointShadowLightIndex_ = -1; pointShadows_ = false; }
    bool pointShadowEnabled() const { return pointShadows_; }
    CubeShadowMap* pointShadowMap() { return pointShadows_ ? &cubeShadow_ : nullptr; }
    /// @brief Depth range of the point-light cube map.
    void setPointShadowRange(float zn, float zf) { pointShadowNear_ = zn; pointShadowFar_ = zf; }
    // --- F46: screen-space contact shadows in the lighting pass (needs sampleable G-buffer
    // depth; automatically off with MSAA, where depth lives in a renderbuffer) ------------
    bool contactShadowsEnabled = false;
    float contactShadowStrength = 1.f;
    float contactShadowMaxDist = 3.f;    ///< raymarch length in world units
    float contactShadowThickness = 0.2f; ///< depth thickness treated as occluder
    int contactShadowSteps = 16;

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
    void drawVolFog(RenderGraph::PassContext& ctx);
    void drawCopy(RenderGraph::PassContext& ctx, std::string_view in);
    void drawBloomBright(RenderGraph::PassContext& ctx);
    void drawRays(RenderGraph::PassContext& ctx);
    void drawComposite(RenderGraph::PassContext& ctx, std::string_view bloomRes);

    DeferredPipeline pipeline_;
    RenderGraph post_;
    ShadowMap shadowMap_;
    CascadeShadowMap csm_;
    CubeShadowMap cubeShadow_; ///< F46: point-light cube shadow map
    void syncShadowTargets();  ///< F46: (re)create shadow targets when size/filter/moments changed
    int width_ = 0, height_ = 0, msaa_ = 0;
    const SceneView* curScene_ = nullptr;
    unsigned frameIndex_ = 0;
    std::string bloomLast_; ///< bloom resource the composite pass reads

    bool bloomEnabled = true; ///< F40: bloom toggle (strength still scales the effect)

    bool shadows_ = false;
    bool csmEnabled_ = false;
    int csmCascades_ = 4;
    float shadowZNear_ = 0.5f, shadowZFar_ = 200.f;
    int shadowSize_ = 2048;
    math::Vec3 shadowCenter_{0.f, 0.f, 0.f};
    float shadowRadius_ = 10.f;
    float shadowStrength_ = 1.f;
    bool pointShadows_ = false;              ///< F46: requested point-light cube shadow
    int pointShadowLightIndex_ = -1;
    int pointShadowSize_ = 1024;
    float pointShadowNear_ = 0.5f, pointShadowFar_ = 100.f;

    Material ssaoMat_, blurMat_, brightMat_, raysMat_, compositeMat_, taaMat_, ssrMat_, dofMat_, volfogMat_, copyMat_;
    VertexArray fsVao_;

    // F39: capabilities captured at create() time; applyCapabilities() clears unsupported
    void applyCapabilities();
    void gate(Effect e, bool& flag);
    int glMajor_ = 0, glMinor_ = 0;
    bool hasBindless_ = false, hasCompute_ = false;
    unsigned warnedMask_ = 0;   ///< effects already warned about (bit = Effect index)
    unsigned requestedMask_ = 0; ///< requested bits for effects without an own field
    int lastLodLevel_ = -1;     ///< F41: last selected LOD level (copied from SceneView)
};

} // namespace fuhgl::render
