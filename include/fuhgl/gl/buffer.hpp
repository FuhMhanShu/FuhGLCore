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
        CopyRead = gl::GL_COPY_READ_BUFFER,
        CopyWrite = gl::GL_COPY_WRITE_BUFFER,
        PixelPack = gl::GL_PIXEL_PACK_BUFFER,
        PixelUnpack = gl::GL_PIXEL_UNPACK_BUFFER,
        DispatchIndirect = gl::GL_DISPATCH_INDIRECT_BUFFER,
        DrawIndirect = gl::GL_DRAW_INDIRECT_BUFFER,
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
    /// @brief Maps a range with flags (GL_MAP_* bits) — the streaming path; nullptr on failure.
    /// No-op (nullptr) when the driver lacks glMapBufferRange (GL < 3.0).
    void* mapRange(gl::GLintptr offset, gl::GLsizeiptr length, gl::GLbitfield access) const;
    /// @brief Flushes a previously mapped range (only with GL_MAP_FLUSH_EXPLICIT_BIT).
    void flushMappedRange(gl::GLintptr offset, gl::GLsizeiptr length) const;
    bool unmap() const;
    /// @brief Invalidates the whole store (driver may orphan the buffer).
    void invalidateData() const;
    /// @brief Invalidates a sub-range of the store.
    void invalidateSubData(gl::GLintptr offset, gl::GLsizeiptr length) const;
    /// @brief Reads back a region into host memory (binds the buffer first).
    void getSubData(gl::GLintptr offset, gl::GLsizeiptr size, void* data) const;
    /// @brief Queries a buffer parameter (GL_BUFFER_SIZE / GL_BUFFER_USAGE / ...), 0 if unsupported.
    gl::GLint parameteri(gl::GLenum pname) const;
    gl::GLint64 parameteri64(gl::GLenum pname) const;
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
