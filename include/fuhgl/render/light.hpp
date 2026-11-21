#pragma once

/// @file light.hpp
/// @brief Light sources (directional / point / spot) for the built-in lighting model
/// and the std140 UBO block layout the pipeline shaders consume.
///
/// Light is a plain value type (documented exception to PIMPL, like Solid): it holds
/// only math state, no GL resources. The first three members stay in the historical
/// order (position, color, intensity) so aggregate initialization keeps working.

#include "../math.hpp"

#include <vector>

namespace fuhgl::render {

/// @brief Maximum number of lights in the built-in LightBlock UBO.
inline constexpr int kMaxLights = 64;

/// @brief Kind of a Light (stored in LightBlock::dirType[i].w).
enum class LightType : int {
    Point = 0,       ///< position, radial attenuation
    Directional = 1, ///< direction only, constant intensity (sun)
    Spot = 2,        ///< position + direction + inner/outer cone
};

/// @brief One light source.
///
/// Aggregate-compatible: `Light{{3,3,4}, {1,.9,.7}, 25.f}` is a point light.
/// Prefer the factories below for the other types.
class Light {
public:
    math::Vec3 position{0.f};              ///< point/spot origin
    math::Vec3 color{1.f};                 ///< linear RGB multiplier
    float intensity = 10.f;                ///< point/spot: radiant intensity; directional: irradiance scale
    LightType type = LightType::Point;
    math::Vec3 direction{0.f, -1.f, 0.f};  ///< travel direction (directional) or aim (spot); normalized internally
    float attenuationConstant = 1.f;       ///< atten = intensity / (c + l*d + q*d^2)
    float attenuationLinear = 0.35f;
    float attenuationQuadratic = 0.12f;
    float range = 0.f;                     ///< 0 = no cutoff; else smooth window to zero at d = range
    float coneInner = 0.5f;                ///< spot inner cone half-angle in radians (full brightness)
    float coneOuter = 0.8f;                ///< spot outer cone half-angle in radians (zero)

    /// @brief Point light at `pos` with a smooth range cutoff (0 = infinite).
    static Light point(const math::Vec3& pos, const math::Vec3& color, float intensity, float range = 0.f);
    /// @brief Directional light (sun): `dir` is the direction the light travels (e.g. {0,-1,0} = straight down).
    static Light directional(const math::Vec3& dir, const math::Vec3& color, float intensity);
    /// @brief Spot light at `pos` aimed along `dir` with inner/outer cone half-angles in radians.
    static Light spot(const math::Vec3& pos, const math::Vec3& dir, const math::Vec3& color, float intensity,
                      float coneInner, float coneOuter);

    /// @brief Distance attenuation at `distance` (includes range window); directional lights return `intensity`.
    float attenuation(float distance) const;
    /// @brief Spot cone factor for a normalized fragment->light direction: 1 inside the inner
    /// cone, 0 outside the outer cone, smooth in between. Non-spot lights return 1.
    float coneFactor(const math::Vec3& toLight) const;
};

/// @brief C++ mirror of the std140 LightBlock (vec4 arrays, 16-byte stride each).
struct LightBlock {
    math::Vec4 posIntensity[kMaxLights]; ///< xyz = position, w = intensity
    math::Vec4 color[kMaxLights];        ///< rgb = color
    math::Vec4 dirType[kMaxLights];      ///< xyz = direction, w = LightType
    math::Vec4 params0[kMaxLights];      ///< x = atten const, y = linear, z = quadratic, w = range
    math::Vec4 params1[kMaxLights];      ///< x = cos(coneInner), y = cos(coneOuter)
};

/// @brief Fills `block` from `lights` (clamped to kMaxLights) and returns the packed count.
int packLightBlock(const std::vector<Light>& lights, LightBlock& block);

} // namespace fuhgl::render
