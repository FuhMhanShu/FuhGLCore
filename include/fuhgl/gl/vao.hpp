#pragma once

/// @file vao.hpp
/// @brief RAII wrapper around a GL vertex array object.

#include "buffer.hpp"
#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief VAO: records vertex attribute pointers and the element buffer binding.
class VertexArray {
public:
    VertexArray();
    ~VertexArray();

    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    VertexArray(VertexArray&&) noexcept;
    VertexArray& operator=(VertexArray&&) noexcept;

    bool create();
    void bind() const;
    static void unbind();

    /// @brief Defines and enables a float vertex attribute (call while bound).
    void setAttrib(gl::GLuint index, gl::GLint size, gl::GLenum type, bool normalized, gl::GLsizei stride, gl::GLintptr offset);
    void enableAttrib(gl::GLuint index);
    void disableAttrib(gl::GLuint index);
    /// @brief Instancing divisor for an attribute (0 = per-vertex).
    void setDivisor(gl::GLuint index, gl::GLuint divisor);
    /// @brief Binds an element (index) buffer to this VAO.
    void setElementBuffer(const Buffer& buffer);

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
