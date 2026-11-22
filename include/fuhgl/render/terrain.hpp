#pragma once

/// @file terrain.hpp
/// @brief Heightmap terrain (F38): generate a landscape mesh from a heightmap —
/// a float grid (e.g. from procedural::heightmap() or an image's grayscale channel).
/// The terrain is a regular grid mesh with finite-difference normals, slope/height
/// based vertex coloring and texturing UVs; heightAt() gives bilinear world-space
/// heights for placing objects on the terrain.

#include "lod.hpp"
#include "mesh.hpp"
#include "procedural.hpp"

#include "../math.hpp"

#include <memory>
#include <vector>

namespace fuhgl::render {

/// @brief Builds a LodMesh (F41) from a heightmap with `levels` grid resolutions
/// (w, w/2, w/4, ... by subsampling). Needs a current GL context (uploads the meshes).
/// Switch distances come from LodMesh::defaultDistances(fovY, radius, fractions).
LodMesh terrainLods(const float* heights, int w, int h,
                    float sizeX, float sizeZ, float heightScale,
                    int levels, float fovYDeg = 60.f);

/// @brief Bilinear sample of a heightmap at normalized (u, v) in [0, 1] (clamped).
float heightmapSample(const float* heights, int w, int h, float u, float v);
/// @brief Same, for a grayscale procedural::Image (red channel / 255).
float heightmapSample(const procedural::Image& img, float u, float v);

/// @brief Builds terrain MeshData from a heightmap: a regular (w x h) vertex grid over
/// [-sizeX/2, sizeX/2] x [-sizeZ/2, sizeZ/2] (centered at the origin), y = height * heightScale.
/// Normals by central differences (clamped at the borders); vertex color interpolates
/// lowColor..highColor with normalized height and shifts to steepColor on steep slopes;
/// uv = world xz * uvScale. Index winding is CCW seen from +Y (pipeline culls back faces).
MeshData heightmapTerrainData(const float* heights, int w, int h,
                              float sizeX, float sizeZ, float heightScale,
                              float uvScale = 0.1f,
                              const math::Vec3& lowColor = {0.23f, 0.35f, 0.16f},
                              const math::Vec3& highColor = {0.48f, 0.44f, 0.38f},
                              const math::Vec3& steepColor = {0.34f, 0.32f, 0.29f});

/// @brief Heightmap terrain mesh (PIMPL): owns the Mesh built from a heightmap copy and
/// offers heightAt() for placing objects. create() needs a current GL context.
class HeightmapTerrain {
public:
    HeightmapTerrain();
    ~HeightmapTerrain();

    HeightmapTerrain(const HeightmapTerrain&) = delete;
    HeightmapTerrain& operator=(const HeightmapTerrain&) = delete;
    HeightmapTerrain(HeightmapTerrain&&) noexcept;
    HeightmapTerrain& operator=(HeightmapTerrain&&) noexcept;

    /// @brief Builds the mesh from a float heightmap (w*h values, row-major, copied).
    bool create(const float* heights, int w, int h, float sizeX, float sizeZ, float heightScale);
    /// @brief Builds the mesh from a grayscale image (red channel in [0,1] = 0..heightScale).
    bool create(const procedural::Image& img, float sizeX, float sizeZ, float heightScale);

    /// @brief World-space terrain height at (x, z) — bilinear, terrain centered at origin.
    float heightAt(float x, float z) const;

    Mesh& mesh() { return mesh_; }
    const Mesh& mesh() const { return mesh_; }
    int gridW() const { return w_; }
    int gridH() const { return h_; }
    float sizeX() const { return sizeX_; }
    float sizeZ() const { return sizeZ_; }
    float heightScale() const { return heightScale_; }
    const std::vector<float>& heights() const { return heights_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Mesh mesh_;
    std::vector<float> heights_;
    int w_ = 0, h_ = 0;
    float sizeX_ = 0.f, sizeZ_ = 0.f, heightScale_ = 1.f;
};

} // namespace fuhgl::render
