#pragma once

/// @file shadow.hpp
/// @brief Shadow mapping prerequisites (F15): a `ShadowMap` (depth-only FBO) that can
/// render the scene depth from a light's point of view, plus helpers to build the
/// light view-projection matrix for directional and spot lights. The pipelines sample
/// the map with PCF when `SceneView::shadowMap` is set.

#include "light.hpp"
#include "material.hpp"
#include "scene.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/texture.hpp"

#include <memory>

namespace fuhgl::render {

/// @brief Depth-only shadow map for one light (directional or spot).
/// Usage: create(size), then per frame `render(scene, lightViewProj(...))`, then set
/// `scene.shadowMap = &map; scene.lightVP = ...; scene.shadowLightIndex = i;`.
class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&&) noexcept;
    ShadowMap& operator=(ShadowMap&&) noexcept;

    /// @brief Creates the depth texture + FBO of `size` x `size` (power of two recommended).
    bool create(int size = 2048);

    /// @brief Renders the depth of all scene items into the map using `lightVP`
    /// (back faces are drawn to reduce shadow acne). Stores lightVP for introspection.
    void render(const SceneView& scene, const math::Mat4& lightVP);

    int size() const { return size_; }
    gl::GLuint textureId() const { return depthTex_.id(); }
    Framebuffer& fbo() { return fbo_; }
    const math::Mat4& lightViewProj() const { return lightVP_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int size_ = 0;
    Framebuffer fbo_;
    Texture depthTex_{Texture::Target::Tex2D};
    Material depthMaterial_; ///< depth-only program (uModel, uLightVP)
    math::Mat4 lightVP_{};
};

/// @brief View matrix looking from the light toward `sceneCenter`.
/// Directional: eye = center - dir*(radius+near); spot: lookAt(position, position+dir).
math::Mat4 lightView(const Light& light, const math::Vec3& sceneCenter, float sceneRadius, float nearPlane = 0.5f);

/// @brief Projection for the light: ortho box of `radius` (directional, aspect 1) or
/// perspective with fov = 2*coneOuter (spot).
math::Mat4 lightProjection(const Light& light, float aspect = 1.f, float nearPlane = 0.5f, float farPlane = 0.f);

/// @brief Combined light view-projection for shadow map rendering (directional + spot).
/// `farPlane` <= 0 picks a default (directional: 2*radius+near, spot: range or 100).
math::Mat4 lightViewProj(const Light& light, const math::Vec3& sceneCenter, float sceneRadius,
                         float nearPlane = 0.5f, float farPlane = 0.f);

} // namespace fuhgl::render
