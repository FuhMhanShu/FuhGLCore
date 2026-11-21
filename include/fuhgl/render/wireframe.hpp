#pragma once

/// @file wireframe.hpp
/// @brief Wireframe renderer: draws solids (triangle edges) and bounding boxes
/// (AABB edges) as GL_LINES — debug views, bounding-volume previews, CSG inspection.

#include "mesh.hpp"
#include "solid.hpp"
#include "../cam.hpp"
#include "../gl/buffer.hpp"
#include "../gl/program.hpp"
#include "../gl/shader.hpp"
#include "../gl/vao.hpp"
#include "../math.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace fuhgl::render {

// --- pure helpers (unit-testable without GL) --------------------------------------

/// @brief The 8 corners of an AABB (order: min corner first, then +x, +y, +z steps).
std::array<math::Vec3, 8> boxCorners(const math::Vec3& mn, const math::Vec3& mx);

/// @brief The 12 edges of a box as index pairs into boxCorners().
extern const std::array<std::array<std::uint32_t, 2>, 12> kBoxEdgeIndices;

/// @brief Unique undirected edges of a triangle mesh as vertex-index pairs
/// (each triangle contributes 3 edges; shared edges are deduplicated).
std::vector<std::pair<std::uint32_t, std::uint32_t>> wireframeEdges(const MeshData& mesh);

// --- wireframe renderer (PIMPL) -----------------------------------------------------

/// @brief Batched line renderer: queue lines on the CPU (no GL needed for queueing),
/// then render() uploads them once and draws a single glDrawArrays(GL_LINES).
///
/// Usage: create() once after a GL context exists; per frame: clear(), add*(...),
/// render(camera, w, h) into the currently bound framebuffer (depth-tested).
class Wireframe {
public:
    Wireframe();
    ~Wireframe();

    Wireframe(const Wireframe&) = delete;
    Wireframe& operator=(const Wireframe&) = delete;
    Wireframe(Wireframe&&) noexcept;
    Wireframe& operator=(Wireframe&&) noexcept;

    /// @brief Creates the line shader and the dynamic vertex buffer.
    bool create();
    bool isCreated() const;

    /// @brief Drops all queued lines.
    void clear();

    /// @brief Queues one world-space line segment.
    void addLine(const math::Vec3& a, const math::Vec3& b, const math::Vec3& color);
    /// @brief Queues the 12 edges of an axis-aligned box.
    void addBox(const math::Vec3& mn, const math::Vec3& mx, const math::Vec3& color);
    /// @brief Queues the AABB edges of a solid (transformed by model).
    void addBoundingBox(const Solid& s, const math::Mat4& model, const math::Vec3& color);
    /// @brief Queues the (deduplicated) triangle edges of mesh data, transformed by model.
    void addMesh(const MeshData& mesh, const math::Mat4& model, const math::Vec3& color);
    /// @brief Meshes a solid (marching tetrahedra) and queues its wireframe.
    void addSolid(const Solid& s, int resolution, const math::Mat4& model, const math::Vec3& color);

    /// @brief Number of queued line segments.
    std::size_t lineCount() const { return verts_.size() / 12; }

    /// @brief Uploads the queued lines and draws them (one draw call) with depth test.
    void render(const Camera& camera, int width, int height);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<float> verts_; ///< 2 vertices per line, 6 floats each (pos3 + color3)
};

} // namespace fuhgl::render
