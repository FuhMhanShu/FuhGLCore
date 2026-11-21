#pragma once

/// @file mesh.hpp
/// @brief Mesh: vertex (+ optional index) buffer + VAO + draw call.

#include "../gl/buffer.hpp"
#include "../gl/vao.hpp"
#include "scene.hpp"

#include <memory>
#include <vector>

namespace fuhgl::render {

/// @brief CPU-side mesh data with the standard vertex layout used by the pipelines
/// and the solid mesher: per vertex 11 floats = pos(3), normal(3), color(3), uv(2).
/// Attribute locations: 0 = pos, 1 = normal, 2 = color, 3 = uv.
struct MeshData {
    std::vector<float> vertices;        ///< 11 floats per vertex
    std::vector<unsigned int> indices;  ///< empty = non-indexed draw
    static constexpr int kFloatsPerVertex = 11;
    static constexpr int kVertexBytes = kFloatsPerVertex * 4;
    std::size_t vertexCount() const { return vertices.size() / kFloatsPerVertex; }
};

/// @brief Geometry: one VAO with a vertex buffer, an optional index buffer and
/// optional per-instance model matrices (instanced rendering, F24).
class Mesh {
public:
    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept;
    Mesh& operator=(Mesh&&) noexcept;

    bool create();

    /// @brief Uploads vertex data and sets up the attribute layout (call while created).
    void setData(const void* data, gl::GLsizeiptr bytes, gl::GLsizei vertexCount,
                 const std::vector<VertexAttrib>& attribs,
                 Buffer::Usage usage = Buffer::Usage::StaticDraw);
    /// @brief Uploads an index buffer and makes draw() use drawElements.
    void setIndices(const void* data, gl::GLsizei indexCount, gl::GLenum indexType = gl::GL_UNSIGNED_INT,
                    Buffer::Usage usage = Buffer::Usage::StaticDraw);
    /// @brief Uploads MeshData (standard pos/normal/color/uv layout, attribs 0..3).
    void setMeshData(const MeshData& data, Buffer::Usage usage = Buffer::Usage::StaticDraw);

    /// @brief Uploads per-instance model matrices (F24 instancing): a divisor-1
    /// mat4 attribute at locations 4..7 (64 bytes stride, column-major Mat4).
    /// After this, draw() issues one instanced draw with `count` instances and the
    /// effective model matrix is uModel * aInstanceModel (pipelines set uInstanced).
    void setInstanceData(const math::Mat4* models, gl::GLsizei count,
                         Buffer::Usage usage = Buffer::Usage::StaticDraw);
    /// @brief Re-uploads instance matrices (count must equal the one passed to setInstanceData).
    void updateInstanceData(const math::Mat4* models, gl::GLsizei count);
    /// @brief Drops instance data: draw() falls back to a single non-instanced draw.
    void clearInstanceData();

    /// @brief Binds the VAO and issues the draw call (instanced when instance data is set).
    void draw() const;

    gl::GLsizei vertexCount() const { return vertexCount_; }
    gl::GLsizei indexCount() const { return indexCount_; }
    gl::GLsizei instanceCount() const { return instanceCount_; }
    gl::GLuint vaoId() const { return vao_.id(); }
    gl::GLenum indexType() const { return indexType_; }
    /// @brief Object-space bounding sphere (xyz = center, w = radius), computed from the
    /// position attribute (index 0) at setData() time; unit sphere at origin by default.
    /// Used by frustum culling (F34).
    math::Vec4 boundingSphere() const { return boundingSphere_; }
    /// @brief Raw vertex/index buffers (e.g. to build a culler VAO referencing this mesh).
    const Buffer& vertexBuffer() const { return vbo_; }
    const Buffer& indexBuffer() const { return ibo_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    gl::GLsizei vertexCount_ = 0;
    gl::GLsizei indexCount_ = 0;
    gl::GLsizei instanceCount_ = 0;
    gl::GLenum indexType_ = gl::GL_UNSIGNED_INT;
    math::Vec4 boundingSphere_{0.f, 0.f, 0.f, 1.f};
    Buffer vbo_{Buffer::Target::Array};
    Buffer ibo_{Buffer::Target::ElementArray};
    Buffer instanceVbo_{Buffer::Target::Array};
    VertexArray vao_;
};

namespace shapes {

/// @brief Box geometry as MeshData (standard layout, 24 vertices, 36 indices,
/// per-face planar UVs, CCW winding against the outward normal on all 6 faces).
MeshData boxData(const math::Vec3& halfExtents, const math::Vec3& color);

/// @brief Quad geometry in the XY plane as MeshData (4 vertices, 6 indices, UVs 0..1).
MeshData quadData(float halfSize, const math::Vec3& color);

/// @brief Fills a Mesh with an axis-aligned box (see boxData).
void box(Mesh& mesh, const math::Vec3& halfExtents, const math::Vec3& color);

/// @brief Fills a Mesh with a quad in the XY plane (see quadData).
void quad(Mesh& mesh, float halfSize, const math::Vec3& color);

} // namespace shapes

} // namespace fuhgl::render
