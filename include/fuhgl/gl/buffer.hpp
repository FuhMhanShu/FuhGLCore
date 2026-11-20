#pragma once

/// @file buffer.hpp
/// @brief RAII wrapper around a GL buffer object (VBO/IBO/UBO/SSBO).

#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief GL buffer object with typed target/usage enums.
class Buffer {
public:
    /// @brief Buffer binding target.
    enum class Target : gl::GLenum {
        Array = gl::GL_ARRAY_BUFFER,
        ElementArray = gl::GL_ELEMENT_ARRAY_BUFFER,
        Uniform = gl::GL_UNIFORM_BUFFER,
        ShaderStorage = gl::GL_SHADER_STORAGE_BUFFER,
    };
    /// @brief Usage hint for the driver.
    enum class Usage : gl::GLenum {
        StaticDraw = gl::GL_STATIC_DRAW,
        DynamicDraw = gl::GL_DYNAMIC_DRAW,
        StreamDraw = gl::GL_STREAM_DRAW,
    };
    /// @brief Access mode for map().
    enum class Access : gl::GLenum {
        ReadOnly = gl::GL_READ_ONLY,
        WriteOnly = gl::GL_WRITE_ONLY,
        ReadWrite = gl::GL_READ_WRITE,
    };

    Buffer();
    /// @brief Creates the buffer object immediately with the given target.
    explicit Buffer(Target target);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;

    bool create(Target target);
    void bind() const;
    /// @brief Unbinds this target (binds 0).
    static void unbind(Target target);

    /// @brief Allocates/replaces the data store.
    void setData(gl::GLsizeiptr size, const void* data, Usage usage) const;
    /// @brief Updates a region of the data store.
    void setSubData(gl::GLintptr offset, gl::GLsizeiptr size, const void* data) const;
    /// @brief Maps the whole buffer; nullptr on failure.
    void* map(Access access) const;
    bool unmap() const;
    /// @brief Binds to an indexed binding point (UBO/SSBO).
    void bindBase(gl::GLuint index) const;
    void bindRange(gl::GLuint index, gl::GLintptr offset, gl::GLsizeiptr size) const;

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
