#pragma once

/// @file culling.hpp
/// @brief GpuCuller (F34): GPU frustum culling of instanced draws + indirect rendering.
///
/// One compute shader (GL 4.3) tests the world-space bounding sphere of every instance
/// (math::Frustum planes + math::transformSphere) against the camera frustum, compacts
/// the visible model matrices into an SSBO (which is simultaneously the divisor-1
/// instance-attribute buffer, locations 4..7 like Mesh::setInstanceData) and writes the
/// visible count into a DrawElementsIndirect/DrawArraysIndirect command via an atomic.
/// draw() then issues a single indirect instanced draw — no per-instance CPU work.

#include "../gl/buffer.hpp"
#include "../gl/program.hpp"
#include "../gl/shader.hpp"
#include "../gl/vao.hpp"
#include "../math/frustum.hpp"
#include "mesh.hpp"

#include <memory>

namespace fuhgl::render {

/// @brief GPU culler for one mesh with up to maxInstances instances (PIMPL, movable).
class GpuCuller {
public:
    GpuCuller();
    ~GpuCuller();

    GpuCuller(const GpuCuller&) = delete;
    GpuCuller& operator=(const GpuCuller&) = delete;
    GpuCuller(GpuCuller&&) noexcept;
    GpuCuller& operator=(GpuCuller&&) noexcept;

    /// @brief Creates the GL objects and compiles the cull compute shader.
    /// Requires GL >= 4.3; returns false otherwise (isReady() == false).
    bool create(int maxInstances);
    bool isReady() const { return ready_; }
    int maxInstances() const { return maxInstances_; }

    /// @brief Selects the mesh drawn per visible instance. The mesh must use the
    /// standard layout (attribs 0..3, MeshData::kVertexBytes stride); indexed or not.
    /// Captures vertex/index counts and the object-space bounding sphere.
    void setMesh(const Mesh& mesh);

    /// @brief Uploads per-instance model matrices (count <= maxInstances). The mesh's
    /// bounding sphere is attached to every instance.
    void setInstances(const math::Mat4* models, int count);
    int instanceCount() const { return instanceCount_; }

    /// @brief Runs the culling compute for the given view-projection: resets the atomic
    /// counter, dispatches, and barriers the command + instance-attribute writes.
    void cull(const math::Mat4& viewProj);

    /// @brief Issues the indirect draw: one instanced draw whose instanceCount is the
    /// GPU-written visible count. The currently bound program must read the per-instance
    /// model matrix from attributes 4..7 (mat4 aInstanceModel, divisor 1).
    void draw() const;

    /// @brief Visible instance count of the last cull() (reads the atomic counter back).
    int lastVisibleCount();

    /// @brief Raw indirect command buffer (GL_DRAW_INDIRECT_BUFFER), e.g. for tests.
    const Buffer& commandBuffer() const { return cmd_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;
    int maxInstances_ = 0;
    int instanceCount_ = 0;
    // mesh info
    gl::GLsizei vertexCount_ = 0;
    gl::GLsizei indexCount_ = 0;
    gl::GLenum indexType_ = gl::GL_UNSIGNED_INT;
    bool indexed_ = false;
    math::Vec4 sphere_{0.f, 0.f, 0.f, 1.f};
    // GL objects
    Program prog_;
    Buffer items_{Buffer::Target::ShaderStorage};  ///< binding 0: Item[]
    Buffer models_{Buffer::Target::ShaderStorage}; ///< binding 1: mat4[] (also attrib buffer)
    Buffer cmd_{Buffer::Target::DrawIndirect};     ///< binding 2: command + atomic counter
    VertexArray vao_;
};

} // namespace fuhgl::render
