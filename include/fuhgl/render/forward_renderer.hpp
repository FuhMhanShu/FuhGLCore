#pragma once

/// @file forward_renderer.hpp
/// @brief ForwardRenderer (F16): the default ready-to-use forward renderer — owns a
/// ForwardPipeline plus the scene-wide settings (lights, ambient, clear color, fog)
/// and an optional shadow map that is re-rendered automatically from the first
/// directional/spot light every frame.

#include "effects.hpp"
#include "light.hpp"
#include "forward_pipeline.hpp"
#include "shadow.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/renderbuffer.hpp"
#include "../gl/texture.hpp"

#include <memory>
#include <vector>

namespace fuhgl::render {

class Environment;

/// @brief Default forward renderer: `render(camera, items, w, h)` does everything —
/// shadow depth pass (if enabled), then the lit forward pass into the own target
/// (create(w, h)) or the currently bound framebuffer (create()).
/// Role vs. render::ForwardPipeline: the *Renderer* owns target + shadows + scene
/// settings and needs one call per frame; the *Pipeline* is the low-level building
/// block underneath (own header: forward_pipeline.hpp).
class ForwardRenderer {
public:
    ForwardRenderer();
    ~ForwardRenderer();

    ForwardRenderer(const ForwardRenderer&) = delete;
    ForwardRenderer& operator=(const ForwardRenderer&) = delete;
    ForwardRenderer(ForwardRenderer&&) noexcept;
    ForwardRenderer& operator=(ForwardRenderer&&) noexcept;

    /// @brief Creates the pipeline. With width/height > 0 an own RGBA8+depth target is
    /// created (use target()/blitToScreen()); with 0 the renderer draws into the
    /// currently bound framebuffer (e.g. the window).
    bool create(int width = 0, int height = 0);
    void resize(int width, int height); ///< only meaningful when created with a size

    // --- scene-wide settings -------------------------------------------------------
    void setLights(const std::vector<Light>& lights) { lights_ = lights; }
    void addLight(const Light& l) { lights_.push_back(l); }
    void clearLights() { lights_.clear(); }
    std::vector<Light>& lights() { return lights_; }
    const std::vector<Light>& lights() const { return lights_; }
    void setAmbient(const math::Vec3& c) { ambient_ = c; }
    void setClearColor(const math::Vec3& c) { clearColor_ = c; }
    /// @brief Exponential distance fog applied in the forward pass (density 0 = off).
    void setFog(const math::Vec3& color, float density) { fogColor_ = color; fogDensity_ = density; }
    /// @brief IBL environment for the ambient term (F29); nullptr = flat ambient only.
    void setEnvironment(const Environment* e) { env_ = e; }

    // --- shadows ---------------------------------------------------------------------
    void enableShadows(int mapSize = 2048);
    void disableShadows() { shadows_ = false; }
    bool shadowsEnabled() const { return shadows_; }
    ShadowMap* shadowMap() { return shadows_ ? &shadowMap_ : nullptr; }
    /// @brief Scene bounds the shadow map covers (center + radius); default origin/10.
    void setShadowSceneBounds(const math::Vec3& center, float radius) { shadowCenter_ = center; shadowRadius_ = radius; }
    void setShadowStrength(float s) { shadowStrength_ = s; }
    /// @brief Camera depth range the cascades cover (F30); default 0.5 .. 200.
    void setShadowZRange(float zn, float zf) { shadowZNear_ = zn; shadowZFar_ = zf; }

    // --- cascaded shadow maps (F30) ---------------------------------------------------
    /// @brief CSM instead of the single ShadowMap: one depth-array layer per cascade,
    /// re-rendered from the first directional light every frame.
    void enableCsmShadows(int mapSize = 2048, int cascades = 4);
    void disableCsmShadows() { csmEnabled_ = false; }
    bool csmShadowsEnabled() const { return csmEnabled_; }
    CascadeShadowMap* cascadeShadowMap() { return csmEnabled_ ? &csm_ : nullptr; }

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

    // --- LOD (F41): DrawItem::lod level selection by camera distance ----------------------
    bool lodEnabled = true;                 ///< false = always LOD level 0
    /// @brief LOD level chosen for the last drawn item that had a LodMesh (-1 = none).
    int lastLodLevel() const { return lastLodLevel_; }

    // --- rendering -------------------------------------------------------------------
    /// @brief Renders one frame: shadow depth pass (when enabled, from the first
    /// directional/spot light) + forward pass with lights/fog/shadows.
    void render(const Camera& camera, const std::vector<DrawItem>& items, int width, int height);

    ForwardPipeline& pipeline() { return pipeline_; }
    Framebuffer* target() { return width_ > 0 ? &target_ : nullptr; }
    /// @brief Blits the own target to the default framebuffer (when created with a size).
    void blitToScreen();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ForwardPipeline pipeline_;
    ShadowMap shadowMap_;
    CascadeShadowMap csm_;
    Framebuffer target_;
    Texture targetTex_{Texture::Target::Tex2D};
    Renderbuffer targetDepth_;
    std::vector<Light> lights_;
    math::Vec3 ambient_{0.06f, 0.06f, 0.07f};
    math::Vec3 clearColor_{0.05f, 0.05f, 0.08f};
    math::Vec3 fogColor_{0.05f, 0.05f, 0.08f};
    float fogDensity_ = 0.f;
    const Environment* env_ = nullptr;
    bool shadows_ = false;
    bool csmEnabled_ = false;
    int csmCascades_ = 4;
    float shadowZNear_ = 0.5f, shadowZFar_ = 200.f;
    int shadowSize_ = 2048;
    math::Vec3 shadowCenter_{0.f, 0.f, 0.f};
    float shadowRadius_ = 10.f;
    float shadowStrength_ = 1.f;
    int width_ = 0, height_ = 0;

    // F39: capabilities captured at create() time; applyCapabilities() clears unsupported
    void applyCapabilities();
    void gate(Effect e, bool& flag);
    int glMajor_ = 0, glMinor_ = 0;
    bool hasBindless_ = false, hasCompute_ = false;
    unsigned warnedMask_ = 0;
    unsigned requestedMask_ = 0;
    int lastLodLevel_ = -1;                 ///< F41
};

} // namespace fuhgl::render
