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

#include <array>
#include <memory>

namespace fuhgl::render {

/// @brief Maximum number of shadow cascades (F30).
inline constexpr int kMaxCascades = 8;

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

// --- cascaded shadow maps (F30) ---------------------------------------------------

/// @brief Practical split scheme (log + uniform mix): splits[0] = zNear, splits[count] = zFar,
/// ascending in between. `lambda` in [0,1] — 0 = uniform, 1 = pure logarithmic.
std::array<float, kMaxCascades + 1> cascadeSplits(float zNear, float zFar, int count, float lambda = 0.5f);

/// @brief Light view-projection for one camera-frustum slice [splitNear, splitFar] (view-space
/// depths). Directional lights get a tightly fitted ortho box around the slice corners;
/// other light types fall back to the full-scene lightViewProj().
math::Mat4 cascadeViewProj(const Light& light, const Camera& camera, float aspect,
                           float splitNear, float splitFar,
                           const math::Vec3& sceneCenter, float sceneRadius);

/// @brief Cascaded shadow map (F30): one depth texture array with one layer per cascade.
/// Usage: create(size, cascades), then per frame
/// `render(scene, camera, light, center, radius, zNear, zFar)` and set `scene.cascades = &map`
/// (the pipelines then sample the array with per-cascade PCF; directional lights only).
class CascadeShadowMap {
public:
    CascadeShadowMap();
    ~CascadeShadowMap();

    CascadeShadowMap(const CascadeShadowMap&) = delete;
    CascadeShadowMap& operator=(const CascadeShadowMap&) = delete;
    CascadeShadowMap(CascadeShadowMap&&) noexcept;
    CascadeShadowMap& operator=(CascadeShadowMap&&) noexcept;

    /// @brief Creates the depth texture array (size x size x cascades) + FBO + depth program.
    bool create(int size = 2048, int cascades = 4);

    /// @brief Renders all cascades: computes the splits from [zNear, zFar], builds one light
    /// VP per cascade and draws the scene depth into each array layer (back faces, like ShadowMap).
    void render(const SceneView& scene, const Camera& camera, const Light& light,
                const math::Vec3& sceneCenter, float sceneRadius, float zNear = 0.5f, float zFar = 200.f);

    int size() const { return size_; }
    int cascadeCount() const { return cascades_; }
    gl::GLuint textureId() const { return depthArray_.id(); } ///< GL_TEXTURE_2D_ARRAY depth map
    /// @brief Light VP of cascade i (valid after render()).
    const math::Mat4& cascadeVP(int i) const { return vps_[i]; }
    /// @brief The count+1 split planes (view-space far depths, ascending) used in the last render().
    const std::array<float, kMaxCascades + 1>& splits() const { return splits_; }
    Framebuffer& fbo() { return fbo_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    int size_ = 0;
    int cascades_ = 0;
    Framebuffer fbo_;
    Texture depthArray_{Texture::Target::Tex2DArray};
    Material depthMaterial_; ///< depth-only program (uModel, uLightVP, uInstanced)
    std::array<math::Mat4, kMaxCascades> vps_{};
    std::array<float, kMaxCascades + 1> splits_{};
};

} // namespace fuhgl::render
