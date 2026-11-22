#pragma once

/// @file render.hpp
/// @brief Umbrella header for the render layer (fuhgl::render).
/// Structure: pipeline.hpp = RenderPipeline base; forward_pipeline.hpp / deferred.hpp
/// = concrete low-level pipelines; forward_renderer.hpp / deferred_renderer.hpp =
/// ready-to-use one-call renderers (owned target + shadows + post FX).

#include "render/scene.hpp"
#include "render/effects.hpp"
#include "render/light.hpp"
#include "render/mesh.hpp"
#include "render/shadow.hpp"
#include "render/shadow_kit.hpp"
#include "render/shadow_glsl.hpp"
#include "render/material.hpp"
#include "render/materials.hpp"
#include "render/environment.hpp"
#include "render/graph.hpp"
#include "render/culling.hpp"
#include "render/procedural.hpp"
#include "render/terrain.hpp"
#include "render/lod.hpp"
#include "render/post_chain.hpp"
#include "render/solid.hpp"
#include "render/pipeline.hpp"
#include "render/forward_pipeline.hpp"
#include "render/deferred.hpp"
#include "render/forward_renderer.hpp"
#include "render/deferred_renderer.hpp"
#include "render/text.hpp"
#include "render/wireframe.hpp"
