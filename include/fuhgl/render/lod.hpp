#pragma once

/// @file lod.hpp
/// @brief Level of detail (F41): a LodMesh holds several meshes for one object (level 0 =
/// highest detail) plus switch distances; the pipelines pick the level from the camera
/// distance per draw item (DrawItem::lod). Selection is CPU-side, so LOD always works
/// (render::Effect::Lod is always supported). Disable with renderer.lodEnabled = false
/// (always level 0).

#include "mesh.hpp"
#include "scene.hpp"

#include "../math.hpp"

#include <vector>

namespace fuhgl::render {

/// @brief Multi-level mesh: levels_[0] is the highest detail; distances_ has
/// levels-1 entries — distance >= distances_[i] switches to level i+1.
/// The meshes must be created (Mesh::create + setData) by the caller; create() only stores.
class LodMesh {
public:
    /// @brief Stores the levels + switch distances (distances.size() == levels.size() - 1,
    /// ascending). No GL needed. The bounding sphere is taken from level 0.
    bool create(std::vector<Mesh> levels, std::vector<float> distances);
    /// @brief Replaces the switch distances (same validation as create()). No GL needed.
    bool setDistances(std::vector<float> distances);

    /// @brief Level index for a camera distance (clamped to [0, levelCount-1]; -1 when empty).
    int selectLevel(float dist) const;
    /// @brief Mesh for a camera distance (nullptr when empty).
    const Mesh* select(float dist) const;

    int levelCount() const { return static_cast<int>(levels_.size()); }
    const Mesh& level(int i) const { return levels_[static_cast<size_t>(i)]; }
    float switchDistance(int i) const { return distances_[static_cast<size_t>(i)]; }
    const std::vector<float>& distances() const { return distances_; }
    /// @brief Object-space bounding sphere of level 0 (xyz = center, w = radius).
    math::Vec4 boundingSphere() const { return bs_; }

    /// @brief Default switch distances from a screen-space heuristic: an object of the
    /// given bounding `radius` at distance d covers ~radius / (d * tan(fovY/2)) of the
    /// screen height; level i+1 kicks in when that fraction drops below fractions[i].
    static std::vector<float> defaultDistances(float fovYRadians, float radius,
                                               const std::vector<float>& fractions);

private:
    std::vector<Mesh> levels_;
    std::vector<float> distances_;
    math::Vec4 bs_{0.f, 0.f, 0.f, 1.f};
};

/// @brief Resolves the mesh to draw for an item: when item.lod is set and lodEnabled,
/// the LOD level for the camera distance (outLevel receives it); otherwise item.mesh.
/// Pure CPU — no GL. DrawItem::lod + SceneView::lodEnabled (F41).
const Mesh* resolveLodMesh(const DrawItem& item, const math::Vec3& camPos, bool lodEnabled,
                           int* outLevel = nullptr);

} // namespace fuhgl::render
