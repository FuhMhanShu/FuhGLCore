#pragma once

/// @file pipeline.hpp
/// @brief Render pipeline base class and the forward renderer.

#include "material.hpp"
#include "mesh.hpp"
#include "scene.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/ubo.hpp"

#include <memory>

namespace fuhgl::render {

/// @brief Common interface of all render pipelines: render one frame from a SceneView.
class RenderPipeline {
public:
    virtual ~RenderPipeline() = default;
    /// @brief Renders the scene (into the pipeline's target or the currently bound FB).
    /// Sets the GL state it needs: depth test (LESS) + back-face culling, so meshes
    /// must use the default CCW front-face winding (as shapes::box/quad do).
    virtual void render(const SceneView& scene) = 0;
};

/// @brief Forward renderer: one pass, per-fragment lighting, straight into the target
/// (own FBO when set via setTarget(), otherwise the currently bound framebuffer).
class ForwardPipeline : public RenderPipeline {
public:
    ForwardPipeline();
    ~ForwardPipeline() override;

    ForwardPipeline(const ForwardPipeline&) = delete;
    ForwardPipeline& operator=(const ForwardPipeline&) = delete;
    ForwardPipeline(ForwardPipeline&&) noexcept;
    ForwardPipeline& operator=(ForwardPipeline&&) noexcept;

    /// @brief Builds the default forward material (shaders compiled internally).
    bool create();

    /// @brief Optional offscreen target; nullptr (default) renders to the bound FB.
    void setTarget(Framebuffer* fbo) { target_ = fbo; }
    /// @brief If a target is set: resolve/blit it to the default framebuffer.
    void blitToScreen();

    /// @brief Default material used for items with material == nullptr.
    /// Custom materials must use the standard uniform names (uModel, uViewProj, uCamPos).
    Material& defaultMaterial() { return defaultMaterial_; }

    void render(const SceneView& scene) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Framebuffer* target_ = nullptr;
    Material defaultMaterial_;
    UniformBuffer lightsUbo_;
};

} // namespace fuhgl::render
