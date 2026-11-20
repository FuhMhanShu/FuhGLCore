#pragma once

/// @file texture.hpp
/// @brief RAII wrapper around a GL texture object (2D in v1).

#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief Texture object. Usage: create(), bind(unit), setImage2D(...), setParameteri(...).
class Texture {
public:
    /// @brief Texture target (v1: 2D only).
    enum class Target : gl::GLenum { Tex2D = gl::GL_TEXTURE_2D };

    Texture();
    explicit Texture(Target target);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    bool create(Target target);
    /// @brief Binds to a texture unit (default GL_TEXTURE0) on the texture's target.
    void bind(gl::GLenum unit = gl::GL_TEXTURE0) const;
    static void unbind(Target target);

    /// @brief Allocates and uploads a full mip level 0 image.
    void setImage2D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                    gl::GLenum format, gl::GLenum type, const void* pixels) const;
    /// @brief Updates a sub-rectangle of an existing level.
    void setSubImage2D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLsizei width, gl::GLsizei height,
                       gl::GLenum format, gl::GLenum type, const void* pixels) const;
    void generateMipmap() const;
    void setParameteri(gl::GLenum pname, gl::GLint param) const;
    void setParameterf(gl::GLenum pname, gl::GLfloat param) const;

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
