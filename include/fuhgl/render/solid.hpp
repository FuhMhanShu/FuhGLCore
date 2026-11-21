#pragma once

/// @file solid.hpp
/// @brief Abstract solid bodies ("Körper") + CSG (constructive solid geometry).
///
/// A Solid is defined analytically by an inside() test plus an AABB — no GL handles,
/// so solids are lightweight polymorphic value types (no PIMPL, like math/DeviceInfo).
/// Meshes are extracted on demand with marching tetrahedra (buildMeshData/buildMesh).
///
/// CSG via shared_ptr composition:
/// @code
///   auto part = makeCube({1.2f,1.2f,1.2f}) - makeSphere(0.9f);   // cube with spherical cut
///   auto both = makeTorus(0.8f, 0.3f) | makeCylinder(0.4f, 2.f); // union
///   render::Mesh mesh = part->buildMesh(48, {0.8f, 0.3f, 0.2f});
/// @endcode

#include "mesh.hpp"
#include "../math.hpp"
#include "../math/frustum.hpp"

#include <memory>

namespace fuhgl::render {

/// @brief Abstract solid body: analytic inside() test + axis-aligned bounding box.
class Solid {
public:
    virtual ~Solid() = default;

    /// @brief True if the point is inside the body (local space, body centered at origin).
    virtual bool inside(const math::Vec3& p) const = 0;
    virtual math::Vec3 aabbMin() const = 0;
    virtual math::Vec3 aabbMax() const = 0;

    /// @brief Extracts a triangle mesh via marching tetrahedra (pure CPU, unit-testable).
    /// @param resolution voxels per longest AABB edge
    /// @param color per-vertex color written into the mesh
    MeshData buildMeshData(int resolution = 32,
                           const math::Vec3& color = {0.75f, 0.75f, 0.78f}) const;

    /// @brief Builds MeshData and uploads it into a render::Mesh (needs a GL context).
    Mesh buildMesh(int resolution = 32,
                   const math::Vec3& color = {0.75f, 0.75f, 0.78f}) const;
};

using SolidPtr = std::shared_ptr<Solid>;

// --- primitives (local space, centered at origin) -------------------------------

/// @brief Axis-aligned cube with the given half extents.
SolidPtr makeCube(const math::Vec3& halfExtents);
/// @brief Sphere of the given radius.
SolidPtr makeSphere(float radius);
/// @brief Cylinder along the Y axis (radius, height).
SolidPtr makeCylinder(float radius, float height);
/// @brief Cone along the Y axis, base at -height/2 (radius = base), tip at +height/2.
SolidPtr makeCone(float radius, float height);
/// @brief Torus around the Y axis (major radius R in XZ, minor radius r).
SolidPtr makeTorus(float majorRadius, float minorRadius);

/// @brief Returns a copy of the solid moved by `offset` (useful to stack CSG parts).
SolidPtr translated(const SolidPtr& s, const math::Vec3& offset);

// --- bounding volumes ---------------------------------------------------------------

/// @brief Kind of bounding volume to build around a solid.
enum class BoundingVolume {
    Box,        ///< axis-aligned bounding box (exact AABB, 8 corners / 12 tris)
    Sphere,     ///< bounding sphere (AABB center, half-diagonal radius)
    Octahedron, ///< low-poly variant: octahedron on the AABB axes (6 verts / 8 tris)
};

/// @brief Exact axis-aligned bounding box of the solid (as a Solid itself).
SolidPtr makeBoundingBox(const Solid& s);
/// @brief Bounding sphere of the solid (center = AABB center, radius = half diagonal).
SolidPtr makeBoundingSphere(const Solid& s);
/// @brief Low-poly bounding volume: octahedron spanning the AABB axes.
SolidPtr makeBoundingOctahedron(const Solid& s);
/// @brief Bounding volume of the given kind around the solid.
SolidPtr makeBoundingVolume(const Solid& s, BoundingVolume kind);

/// @brief Frustum-vs-solid test via the AABB (for culling; needs math::Frustum).
bool intersects(const math::Frustum& frustum, const Solid& s);

// --- CSG operators ----------------------------------------------------------------

/// @brief Union: inside either body.
SolidPtr operator|(const SolidPtr& a, const SolidPtr& b);
/// @brief Intersection: inside both bodies.
SolidPtr operator&(const SolidPtr& a, const SolidPtr& b);
/// @brief Difference: inside a but not inside b.
SolidPtr operator-(const SolidPtr& a, const SolidPtr& b);

} // namespace fuhgl::render
