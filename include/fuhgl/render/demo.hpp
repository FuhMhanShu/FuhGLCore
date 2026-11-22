#pragma once

/// @file demo.hpp
/// @brief DemoScene (F43): the showcase landscape as a reusable, renderer-agnostic demo
/// scene — the renderers "as a demo". One DemoScene feeds both ready-to-use renderers:
/// `configure(ForwardRenderer&)` / `configure(DeferredRenderer&)` apply the matching
/// effect stack (CSM shadows, post FX, IBL at night), `fillScene()` provides the per-frame
/// SceneView. `examples/demo` runs the same scene through either renderer
/// (`--renderer forward|deferred`); `examples/showcase` is the deferred-only thin wrapper
/// used for the README hero images. See tutorial 45.

#include "scene.hpp"

#include "../math.hpp"

#include <cctype>
#include <memory>
#include <string>
#include <string_view>

namespace fuhgl {
class FirstPersonCamera;
}

namespace fuhgl::render {

class ForwardRenderer;
class DeferredRenderer;
class Environment;

/// @brief The three demo looks (same as `examples/showcase --mode`).
enum class DemoMode {
    Day,    ///< high warm sun, blue gradient sky, morning mist
    Sunset, ///< sun low in front of the camera: long shadows, strong god rays, warm haze
    Night,  ///< dark sky + baked IBL environment, emissive lamps (SSR reflections)
};

/// @brief Parses a mode name ("day"/"sunset"/"night", case-insensitive); unknown -> fallback.
inline DemoMode demoModeFromName(std::string_view name, DemoMode fallback = DemoMode::Day) {
    auto lower = [](std::string_view s) {
        std::string out(s);
        for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    };
    const std::string n = lower(name);
    if (n == "sunset") return DemoMode::Sunset;
    if (n == "night") return DemoMode::Night;
    if (n == "day") return DemoMode::Day;
    return fallback;
}
/// rocks/monolith, instanced grass, PBR water — one scene, both renderers.
/// All GL objects are owned by the DemoScene (PIMPL); create() needs a current context.
class DemoScene {
public:
    DemoScene();
    ~DemoScene();

    DemoScene(const DemoScene&) = delete;
    DemoScene& operator=(const DemoScene&) = delete;
    DemoScene(DemoScene&&) noexcept;
    DemoScene& operator=(DemoScene&&) noexcept;

    /// @brief Builds all meshes/materials/textures/lights for the given look (night also
    /// bakes the IBL environment). Needs a current GL context. Returns false on failure.
    bool create(DemoMode mode = DemoMode::Day);
    DemoMode mode() const;

    /// @brief Terrain height at world (x, z) — shared by the mesh, object placement and
    /// the camera start pose.
    float heightAt(float x, float z) const;
    /// @brief Applies the recommended first-person start pose (looks at monolith + pond).
    void applyStartPose(fuhgl::FirstPersonCamera& cam) const;
    /// @brief Focus distance for depth of field: start pose -> monolith.
    float dofFocusDistance() const;

    /// @brief Fills a SceneView (camera/width/height set by the caller) with the demo
    /// items, lights, fog, clear/ambient colors and (night) the IBL environment.
    /// The items point at internal meshes — the SceneView must not outlive the DemoScene.
    void fillScene(SceneView& scene) const;

    /// @brief Demo configuration for the forward renderer: CSM shadows, lights, ambient,
    /// clear color, distance fog, IBL (night) and the HDR post chain (bloom, god rays,
    /// TAA, DoF, volumetric fog) via enablePostFX().
    void configure(ForwardRenderer& r) const;
    /// @brief Demo configuration for the deferred renderer: CSM shadows, SSAO, HDR
    /// lighting with sun disk, bloom, god rays, TAA, SSR, DoF, volumetric fog, sky
    /// gradient, IBL (night).
    void configure(DeferredRenderer& r) const;

    /// @brief IBL environment baked in night mode (nullptr otherwise).
    const Environment* environment() const;
    /// @brief HUD title for the current look ("day" / "sunset" / "night").
    const char* title() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl::render
