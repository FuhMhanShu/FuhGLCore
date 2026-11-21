#pragma once

/// @file pipeline.hpp
/// @brief RenderPipeline: the common base class of all render pipelines.
/// Concrete pipelines live in their own headers — forward_pipeline.hpp
/// (ForwardPipeline) and deferred.hpp (DeferredPipeline). Ready-to-use one-call
/// renderers that own a target + shadows + post-processing: forward_renderer.hpp /
/// deferred_renderer.hpp (ForwardRenderer / DeferredRenderer).

#include "material.hpp"
#include "mesh.hpp"
#include "scene.hpp"

namespace fuhgl::render {

/// @brief Common interface of all render pipelines: render one frame from a SceneView.
/// Role vs. the *Renderer classes: a pipeline is a low-level building block (you bind
/// a target and call render(scene) yourself); ForwardRenderer/DeferredRenderer wrap a
/// pipeline with an owned target, automatic shadow pass and post-processing chain.
class RenderPipeline {
public:
    virtual ~RenderPipeline() = default;
    /// @brief Renders the scene (into the pipeline's target or the currently bound FB).
    /// Sets the GL state it needs: depth test (LESS) + back-face culling, so meshes
    /// must use the default CCW front-face winding (as shapes::box/quad do).
    virtual void render(const SceneView& scene) = 0;
};

} // namespace fuhgl::render
