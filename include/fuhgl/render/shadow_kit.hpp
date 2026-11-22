#pragma once

/// @file shadow_kit.hpp
/// @brief F45 — building blocks for advanced shadow techniques.
///
/// These classes are deliberately NOT wired into ForwardRenderer/DeferredRenderer:
/// they are the technical prerequisites so an application using FuhGLCore can
/// implement realistic light/shadow effects itself (soft shadows via PCSS/VSM,
/// stabilized cascades, point-light cube shadows, multi-light atlases,
/// contact shadows, ...). Compose them with your own passes and shaders; the
/// matching GLSL snippets live in `render/shadow_glsl.hpp`.
///
/// Provided pieces:
/// - `ShadowTarget`   — generic depth target: 2D / 2D-array / cube, optionally with
///                      an RGBA32F "moments" color attachment for VSM/ESM soft shadows
/// - `CubeShadowMap`  — cube depth map for one point light (6 faces)
/// - `ShadowAtlas`    — 2D-array of shadow layers, one per shadow-casting light
/// - `pointLightFaceVPs()`     — the 6 cube-face view-projections of a point light
/// - `snapLightVPToTexels()`   — texel-snapping a light VP (kills shadow swimming)
/// - `hemisphereKernel()` / `poissonDisk()` — deterministic sample patterns for
///   SSAO/HBAO/contact-shadow kernels and PCF/PCSS sampling (CPU-side, unit-testable)

#include "light.hpp"
#include "material.hpp"
#include "scene.hpp"

#include "../gl/framebuffer.hpp"
#include "../gl/texture.hpp"

#include <array>
#include <memory>
#include <vector>

namespace fuhgl::render {

/// @brief Geometry kind of a ShadowTarget.
enum class ShadowTargetKind {
    Tex2D,      ///< one 2D depth map (directional/spot light, one cascade)
    Tex2DArray, ///< depth array, one layer per cascade or per shadow-casting light
    Cube,       ///< cube depth map (point light, 6 faces)
};

/// @brief Shadow sampling filter used by the renderers on top of the kit targets
/// (F46). 0 keeps the legacy 3x3 PCF of the built-in pipelines bit-identical.
enum class ShadowFilter {
    Pcf3x3 = 0,   ///< legacy 3x3 PCF on the LINEAR depth texture (default, no moments needed)
    PoissonPcf = 1, ///< poisson-disk PCF on the depth texture (softer edges)
    Pcss = 2,     ///< percentage-closer soft shadows (blocker search + penumbra-sized PCF)
    Vsm = 3,      ///< variance shadow map: render moments, Chebyshev-filtered (soft, some bleeding)
};

/// @brief Generic shadow depth target (PIMPL): depth texture (+ optional RGBA32F
/// moments color texture for variance shadow maps) + FBO + depth-only materials.
///
/// Usage: `create(kind, size, layers, moments)`, then per frame
/// `render(scene, lightVP, layerOrFace)` — for `Cube`, pass the face index 0..5
/// (order GL_TEXTURE_CUBE_MAP_POSITIVE_X .. NEGATIVE_Z). Sample the result with the
/// GLSL snippets from `render/shadow_glsl.hpp` in your own shaders.
class ShadowTarget {
public:
    ShadowTarget();
    ~ShadowTarget();

    ShadowTarget(const ShadowTarget&) = delete;
    ShadowTarget& operator=(const ShadowTarget&) = delete;
    ShadowTarget(ShadowTarget&&) noexcept;
    ShadowTarget& operator=(ShadowTarget&&) noexcept;

    /// @brief Creates the target. `layers` = array layers (Tex2DArray) or 6 (Cube, forced).
    /// `moments` = additionally allocate an RGBA32F color texture and render depth
    /// moments (d, d^2) into it for VSM soft shadows.
    bool create(ShadowTargetKind kind, int size = 2048, int layers = 1, bool moments = false);

    /// @brief Renders the scene depth from `vp` into `layerOrFace`
    /// (array layer, or cube face 0..5 when kind() == Cube; ignored for Tex2D).
    /// Back faces are drawn (less shadow acne), like ShadowMap::render().
    void render(const SceneView& scene, const math::Mat4& vp, int layerOrFace = 0);

    ShadowTargetKind kind() const { return kind_; }
    int size() const { return size_; }
    int layers() const { return layers_; }
    bool moments() const { return moments_; }
    gl::GLuint textureId() const { return depthTex_.id(); } ///< depth texture (2D / array / cube)
    /// @brief RGBA32F moments texture when moments() was requested, else 0.
    gl::GLuint momentsTextureId() const { return moments_ ? momentsTex_.id() : 0; }
    Framebuffer& fbo() { return fbo_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ShadowTargetKind kind_ = ShadowTargetKind::Tex2D;
    int size_ = 0;
    int layers_ = 0;
    bool moments_ = false;
    Framebuffer fbo_;
    Texture depthTex_;
    Texture momentsTex_;
    Material depthMaterial_;   ///< depth-only program (hard shadows)
    Material momentsMaterial_; ///< moments program (VSM: writes vec4(d, d^2, 0, 1))
};

/// @brief The 6 cube-face view-projections of a point light at `pos`
/// (90° fov, `aspect`, [zNear, zFar]), ordered GL_TEXTURE_CUBE_MAP_POSITIVE_X ..
/// NEGATIVE_Z — matching the face index of `ShadowTarget::render(..., face)`.
std::array<math::Mat4, 6> pointLightFaceVPs(const math::Vec3& pos, float zNear, float zFar, float aspect = 1.f);

/// @brief Snaps a light view-projection so that `referenceWorldPos` (e.g. the scene
/// center) lands exactly on a texel center of a `mapSize` x `mapSize` shadow map.
/// Removes the sub-texel swimming of shadow edges when the camera moves (CSM
/// stabilization). Works for ortho and perspective light projections.
math::Mat4 snapLightVPToTexels(const math::Mat4& lightVP, int mapSize, const math::Vec3& referenceWorldPos);

/// @brief Cube shadow map for one point light: wraps a `ShadowTarget` of kind Cube
/// and the 6 face VPs. Usage: `create(size, moments)`, per frame
/// `render(scene, pointLight, zNear, zFar)`, then sample `textureId()` as
/// `samplerCube` in your shader (normalize depth by `farPlane()`).
class CubeShadowMap {
public:
    CubeShadowMap();
    ~CubeShadowMap();

    CubeShadowMap(const CubeShadowMap&) = delete;
    CubeShadowMap& operator=(const CubeShadowMap&) = delete;
    CubeShadowMap(CubeShadowMap&&) noexcept;
    CubeShadowMap& operator=(CubeShadowMap&&) noexcept;

    /// @brief Creates the cube depth target (size x size x 6 faces, optional VSM moments).
    bool create(int size = 1024, bool moments = false);

    /// @brief Renders all 6 faces from the point light's position (90° fov per face).
    void render(const SceneView& scene, const Light& pointLight, float zNear = 0.5f, float zFar = 100.f);

    int size() const { return target_.size(); }
    bool moments() const { return target_.moments(); }
    gl::GLuint textureId() const { return target_.textureId(); }
    gl::GLuint momentsTextureId() const { return target_.momentsTextureId(); }
    Framebuffer& fbo() { return target_.fbo(); }
    /// @brief Face VP of face i (valid after render()).
    const math::Mat4& faceVP(int i) const { return faceVPs_[i]; }
    float nearPlane() const { return zNear_; }
    float farPlane() const { return zFar_; } ///< normalize cube depth by this in the shader

private:
    ShadowTarget target_;
    std::array<math::Mat4, 6> faceVPs_{};
    float zNear_ = 0.5f;
    float zFar_ = 100.f;
};

/// @brief Atlas of 2D shadow-map layers — one layer per shadow-casting light, so
/// several lights can cast shadows in the same pass loop. Wraps a `ShadowTarget`
/// of kind Tex2DArray and stores one light VP per layer.
///
/// Usage: `create(size, layers, moments)`, per frame
/// `renderLight(scene, layer, light, center, radius, snap)` (computes the light VP
/// via `lightViewProj()` and optionally texel-snaps it) or `renderLayer(scene, layer, vp)`
/// with a precomputed VP; sample `textureId()` as `sampler2DArray` with layer =
/// your light index (see `render/shadow_glsl.hpp`).
class ShadowAtlas {
public:
    ShadowAtlas();
    ~ShadowAtlas();

    ShadowAtlas(const ShadowAtlas&) = delete;
    ShadowAtlas& operator=(const ShadowAtlas&) = delete;
    ShadowAtlas(ShadowAtlas&&) noexcept;
    ShadowAtlas& operator=(ShadowAtlas&&) noexcept;

    /// @brief Creates the atlas: `layers` depth layers of size x size (optional VSM moments).
    bool create(int size = 2048, int layers = 4, bool moments = false);

    /// @brief Renders one light into `layer`: builds the light VP (directional/spot via
    /// `lightViewProj()`), optionally texel-snaps it against `center`, stores it in
    /// vp(layer) and draws the scene depth into the layer.
    void renderLight(const SceneView& scene, int layer, const Light& light,
                     const math::Vec3& center, float radius,
                     bool snap = true, float zNear = 0.5f, float zFar = 0.f);

    /// @brief Renders one layer with a precomputed light VP (e.g. a snapped cascade VP).
    void renderLayer(const SceneView& scene, int layer, const math::Mat4& vp);

    int size() const { return target_.size(); }
    int layers() const { return target_.layers(); }
    bool moments() const { return target_.moments(); }
    gl::GLuint textureId() const { return target_.textureId(); }
    gl::GLuint momentsTextureId() const { return target_.momentsTextureId(); }
    Framebuffer& fbo() { return target_.fbo(); }
    /// @brief Stored light VP of `layer` (set by renderLight()/setVP()).
    const math::Mat4& vp(int layer) const { return vps_[layer]; }
    void setVP(int layer, const math::Mat4& vp) { vps_[layer] = vp; }

private:
    ShadowTarget target_;
    std::array<math::Mat4, 64> vps_{};
};

/// @brief Deterministic hemisphere sample kernel (unit vectors, z in [0,1], biased
/// toward the origin) for SSAO/HBAO/contact-shadow kernels. Same seed -> same kernel.
std::vector<math::Vec3> hemisphereKernel(int samples, unsigned seed = 0);

/// @brief Deterministic Poisson-disk sample pattern in the unit circle (radius <= 1),
/// for PCF/PCSS sampling patterns. Same seed -> same disk.
std::vector<math::Vec2> poissonDisk(int samples, unsigned seed = 0);

} // namespace fuhgl::render
