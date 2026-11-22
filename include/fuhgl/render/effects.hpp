#pragma once

/// @file effects.hpp
/// @brief Effect registry + capability mapping (F39/F40): which render-layer features the
/// current GPU can run, and a uniform on/off API shared by both renderers.
/// The mapping `effectSupported()` is a pure function of (GL version, bindless, compute)
/// so it is unit-testable without a GL context.

#include "../gl/info.hpp"

#include <cstdio>

namespace fuhgl::render {

/// @brief All toggleable render-layer features/effects.
enum class Effect {
    Shadows,          ///< single depth shadow map (F15)
    CsmShadows,       ///< cascaded shadow maps, depth texture array (F30)
    Ssao,             ///< screen-space ambient occlusion (F17, deferred only)
    Ibl,              ///< image-based lighting environment (F29)
    Bloom,            ///< HDR bloom (deferred post chain; forward post chain F42)
    GodRays,          ///< crepuscular rays from the sun (deferred post chain; forward F42)
    Taa,              ///< temporal anti-aliasing (F32)
    Ssr,              ///< screen-space reflections (F33, needs G-buffer: deferred only)
    DoF,              ///< depth of field (F36)
    VolFog,           ///< volumetric fog / light shafts (F37)
    Lod,              ///< level of detail selection (F41, CPU-side)
    HeightmapTerrain, ///< heightmap terrain mesh (F38, CPU-side)
    GpuCulling,       ///< compute-shader frustum culling (F34, GL >= 4.3)
    BindlessTextures, ///< GL_ARB_bindless_texture (F35)
    PointShadows,     ///< cube shadow map for one point light (F46)
    ContactShadows,   ///< screen-space contact shadows in the lighting pass (F46, deferred only)
};

/// @brief Number of Effect values.
constexpr int kEffectCount = 16;

/// @brief Human-readable name of an Effect (for logs/warnings).
inline const char* effectName(Effect e) {
    switch (e) {
        case Effect::Shadows: return "shadows";
        case Effect::CsmShadows: return "CSM shadows";
        case Effect::Ssao: return "SSAO";
        case Effect::Ibl: return "IBL";
        case Effect::Bloom: return "bloom";
        case Effect::GodRays: return "god rays";
        case Effect::Taa: return "TAA";
        case Effect::Ssr: return "SSR";
        case Effect::DoF: return "DoF";
        case Effect::VolFog: return "volumetric fog";
        case Effect::Lod: return "LOD";
        case Effect::HeightmapTerrain: return "heightmap terrain";
        case Effect::GpuCulling: return "GPU culling";
        case Effect::BindlessTextures: return "bindless textures";
        case Effect::PointShadows: return "point shadows";
        case Effect::ContactShadows: return "contact shadows";
    }
    return "unknown";
}

/// @brief Pure capability mapping: can this GPU run the effect?
/// All built-in render shaders are GLSL 330, so the GPU-implemented effects need GL >= 3.3
/// (CSM additionally needs texture arrays, core since 3.0); GPU culling needs compute
/// shaders (GL >= 4.3 or ARB_compute_shader); bindless needs GL_ARB_bindless_texture;
/// LOD and heightmap terrain are CPU-side and always supported.
inline bool effectSupported(Effect e, int glMajor, int glMinor, bool hasBindless, bool hasCompute) {
    switch (e) {
        case Effect::Shadows:
        case Effect::CsmShadows:
        case Effect::Ssao:
        case Effect::Ibl:
        case Effect::Bloom:
        case Effect::GodRays:
        case Effect::Taa:
        case Effect::Ssr:
        case Effect::DoF:
        case Effect::VolFog:
        case Effect::PointShadows:
        case Effect::ContactShadows:
            return glMajor > 3 || (glMajor == 3 && glMinor >= 3);
        case Effect::GpuCulling:
            return hasCompute || glMajor > 4 || (glMajor == 4 && glMinor >= 3);
        case Effect::BindlessTextures:
            return hasBindless;
        case Effect::Lod:
        case Effect::HeightmapTerrain:
            return true; // CPU-side
    }
    return false;
}

/// @brief Same mapping from a queried DeviceInfo (call with a current context).
inline bool effectSupported(Effect e, const gl::DeviceInfo& info) {
    return effectSupported(e, info.version.major, info.version.minor,
                           info.hasExtension("GL_ARB_bindless_texture"), info.features.compute);
}

/// @brief Prints a one-time-style warning that an effect was disabled because the GPU
/// does not support it (graceful degradation, F39).
inline void warnUnsupported(const char* rendererName, Effect e, int glMajor, int glMinor) {
    std::fprintf(stderr, "%s: effect '%s' disabled — not supported by this GPU (GL %d.%d)\n",
                 rendererName, effectName(e), glMajor, glMinor);
}

} // namespace fuhgl::render
