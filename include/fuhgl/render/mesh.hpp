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

/// @brief Geometry: one VAO with a vertex buffer and an optional index buffer.
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

    /// @brief Binds the VAO and issues the draw call.
    void draw() const;

    gl::GLsizei vertexCount() const { return vertexCount_; }
    gl::GLsizei indexCount() const { return indexCount_; }
    gl::GLuint vaoId() const { return vao_.id(); }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    gl::GLsizei vertexCount_ = 0;
    gl::GLsizei indexCount_ = 0;
    gl::GLenum indexType_ = gl::GL_UNSIGNED_INT;
    Buffer vbo_{Buffer::Target::Array};
    Buffer ibo_{Buffer::Target::ElementArray};
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
