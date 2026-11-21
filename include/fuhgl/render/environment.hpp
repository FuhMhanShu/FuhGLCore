#pragma once

/// @file environment.hpp
/// @brief Environment (F29): image-based lighting (IBL). A source cubemap is baked on
/// the CPU (deterministic, no GL needed for the math) into an irradiance cubemap, a
/// GGX-prefiltered environment cubemap with a mip chain (roughness per mip level) and a
/// 2D BRDF integration LUT; `render::Environment` uploads them as GL textures and the
/// pipelines sample them when `SceneView::env` is set (texture units
/// kTextureUnitIrradiance / kTextureUnitPrefilter / kTextureUnitBrdfLut).

#include "../gl/texture.hpp"
#include "../math.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace fuhgl::render {

/// @brief Raw cubemap on the CPU: 6 square faces of RGBA float pixels.
struct CubemapData {
    int size = 0;               ///< face edge length in pixels (square faces)
    std::vector<float> face[6]; ///< RGBA float, size*size*4 values per face (face order +X,-X,+Y,-Y,+Z,-Z)
    /// @brief True when size > 0 and every face holds size*size*4 floats.
    bool valid() const {
        if (size <= 0) return false;
        const std::size_t need = static_cast<std::size_t>(size) * size * 4;
        for (int f = 0; f < 6; ++f)
            if (face[f].size() != need) return false;
        return true;
    }
};

/// @brief Reads back a cubemap texture (level 0) into RGBA float faces (needs a GL context).
CubemapData readCubemap(const Texture& cube);

// --- pure math helpers (unit-testable, no GL) -----------------------------------

/// @brief World direction of cubemap texel (face, u, v) with u/v in [0,1] across the
/// face (GL cubemap layout: +X,-X,+Y,-Y,+Z,-Z; v = 0 at the top row).
math::Vec3 cubemapTexelDir(int face, float u, float v);
/// @brief Nearest-neighbor cubemap sample (RGB).
math::Vec3 sampleCubemap(const CubemapData& src, const math::Vec3& dir);
/// @brief Van der Corput radical inverse (base 2).
float radicalInverse(std::uint32_t bits);
/// @brief Hammersley point i of n in [0,1)^2.
math::Vec2 hammersley(std::uint32_t i, std::uint32_t n);
/// @brief GGX (Trowbridge-Reitz) importance sample around the normal n.
math::Vec3 importanceSampleGGX(const math::Vec2& xi, float roughness, const math::Vec3& n);
/// @brief Schlick-GGX geometry term with explicit k.
float geometrySchlickGGX(float ndotx, float k);

/// @brief CPU-baked IBL data (value type, no GL handles).
struct EnvironmentBake {
    int irradianceSize = 0;                 ///< irradiance face edge length
    std::vector<float> irradianceFace[6];   ///< RGB float, size^2*3 per face
    int prefilterSize = 0;                  ///< prefilter base face edge length
    int prefilterLevels = 0;                ///< mip levels of the prefilter cubemap
    std::vector<float> prefilterFaceLevel[6]; ///< concatenated mip levels, level m at
                                              ///< offset sum_{l<m} (size>>l)^2 * 3, RGB float
    int brdfLutSize = 0;                    ///< BRDF LUT edge length
    std::vector<float> brdfLut;             ///< RG float, size^2*2 (R = scale, G = bias)
    /// @brief True when all arrays are consistently sized.
    bool valid() const;
};

/// @brief Bakes IBL data from a source cubemap (deterministic, pure CPU).
/// irradianceSize = irradiance face size, prefilterSize = prefilter base face size
/// (mip chain down to 1x1), brdfLutSize = BRDF LUT edge length.
bool bakeEnvironment(const CubemapData& src, EnvironmentBake& out,
                     int irradianceSize = 32, int prefilterSize = 128, int brdfLutSize = 256);

/// @brief IBL environment as GL textures (PIMPL, movable, non-copyable).
/// createFromCubemap() reads back + bakes the source cubemap and uploads the results.
class Environment {
public:
    Environment();
    ~Environment();

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
    Environment(Environment&&) noexcept;
    Environment& operator=(Environment&&) noexcept;

    /// @brief Bakes (CPU) and uploads irradiance + prefilter cubemaps and the BRDF LUT.
    bool createFromCubemap(const Texture& srcCube, int irradianceSize = 32, int prefilterSize = 128,
                           int brdfLutSize = 256);

    const Texture& irradianceMap() const { return irradianceTex_; }
    const Texture& prefilterMap() const { return prefilterTex_; }
    const Texture& brdfLut() const { return brdfLutTex_; }
    /// @brief Highest prefilter mip level (roughness 1); used for textureLod scaling.
    int prefilterMaxLod() const { return prefilterLevels_ - 1; }
    /// @brief Multiplier for the ambient IBL term in the pipelines (uEnvIntensity).
    float intensity = 1.f;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Texture irradianceTex_{Texture::Target::Cube};
    Texture prefilterTex_{Texture::Target::Cube};
    Texture brdfLutTex_{Texture::Target::Tex2D};
    int prefilterLevels_ = 0;
};

} // namespace fuhgl::render
