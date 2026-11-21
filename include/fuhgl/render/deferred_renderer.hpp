#pragma once

/// @file deferred_renderer.hpp
/// @brief DeferredRenderer (F17): the default ready-to-use deferred renderer —
/// G-buffer pass + SSAO + HDR lighting (lights, PCF shadows, fog, sky sun disk),
/// then a post-processing chain: bloom (bright-pass + separable blur), crepuscular
/// "god" rays from the sun's screen position, and ACES tone mapping + gamma into
/// the LDR output. Parallax/bump mapping live in the G-buffer pass (material maps).

#include "deferred.hpp"
#include "light.hpp"
#include "shadow.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/texture.hpp"
#include "../gl/ubo.hpp"
#include "../gl/vao.hpp"

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

    /// @brief Full frame: shadow depth pass (if enabled) -> G-buffer -> SSAO ->
    /// HDR lighting -> bloom -> god rays -> ACES+gamma composite into output().
    void render(const SceneView& scene);

    Framebuffer& output() { return pipeline_.output(); }
    DeferredPipeline& pipeline() { return pipeline_; }
    int width() const { return width_; }
    int height() const { return height_; }
    /// @brief The HDR scene texture before tone mapping (debug/introspection).
    const Texture& hdrTexture() const { return texHdr_; }

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

    // --- shadows (same model as ForwardRenderer) ---------------------------------------
    void enableShadows(int mapSize = 2048);
    void disableShadows() { shadows_ = false; }
    bool shadowsEnabled() const { return shadows_; }
    ShadowMap* shadowMap() { return shadows_ ? &shadowMap_ : nullptr; }
    void setShadowSceneBounds(const math::Vec3& center, float radius) { shadowCenter_ = center; shadowRadius_ = radius; }
    void setShadowStrength(float s) { shadowStrength_ = s; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    bool createPost(int w, int h);
    void runSSAO(const SceneView& scene);
    void runBloom();
    void runRays(const SceneView& scene);
    void runComposite();
    void beginPass(Framebuffer& fbo);
    void endPass();
    void drawFs(Material& mat);

    DeferredPipeline pipeline_;
    ShadowMap shadowMap_;
    int width_ = 0, height_ = 0, msaa_ = 0;

    bool shadows_ = false;
    int shadowSize_ = 2048;
    math::Vec3 shadowCenter_{0.f, 0.f, 0.f};
    float shadowRadius_ = 10.f;
    float shadowStrength_ = 1.f;

    // post resources (RGBA16F HDR chain, RGBA8 AO)
    Framebuffer ssaoFbo_, blurFbo_, hdrFbo_, bloomFboA_, bloomFboB_, raysFbo_;
    Texture texSsao_{Texture::Target::Tex2D};
    Texture texBlur_{Texture::Target::Tex2D};
    Texture texHdr_{Texture::Target::Tex2D};
    Texture texBloomA_{Texture::Target::Tex2D};
    Texture texBloomB_{Texture::Target::Tex2D};
    Texture texRays_{Texture::Target::Tex2D};
    Material ssaoMat_, blurMat_, brightMat_, raysMat_, compositeMat_;
    VertexArray fsVao_;
};

} // namespace fuhgl::render
