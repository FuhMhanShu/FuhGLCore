#pragma once

/// @file mesh.hpp
/// @brief Mesh: vertex (+ optional index) buffer + VAO + draw call.

#include "../gl/buffer.hpp"
#include "../gl/vao.hpp"
#include "scene.hpp"

#include <memory>
#include <vector>

namespace fuhgl::render {

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

/// @brief Fills a Mesh with an axis-aligned box (positions + normals + color,
/// stride 9 floats, 24 vertices, 36 indices).
void box(Mesh& mesh, const math::Vec3& halfExtents, const math::Vec3& color);

/// @brief Fills a Mesh with a fullscreen-triangle-less unit grid? No — a simple
/// colored quad in the XY plane (positions + normals + color, 4 vertices, 6 indices).
void quad(Mesh& mesh, float halfSize, const math::Vec3& color);

} // namespace shapes

} // namespace fuhgl::render
