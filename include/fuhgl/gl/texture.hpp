#pragma once

/// @file texture.hpp
/// @brief RAII wrapper around a GL texture object (2D in v1).

#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief Texture object. Usage: create(), bind(unit), setImage2D(...), setParameteri(...).
class Texture {
public:
    /// @brief Texture target.
    enum class Target : gl::GLenum {
        Tex2D = gl::GL_TEXTURE_2D,
        Tex3D = gl::GL_TEXTURE_3D,
        Tex2DArray = gl::GL_TEXTURE_2D_ARRAY,
        Cube = gl::GL_TEXTURE_CUBE_MAP,
        CubeArray = gl::GL_TEXTURE_CUBE_MAP_ARRAY,
        Tex2DMultisample = gl::GL_TEXTURE_2D_MULTISAMPLE,
        Tex2DMultisampleArray = gl::GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
    };
    /// @brief Cube map face.
    enum class CubeFace : gl::GLenum {
        PosX = gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        NegX = gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        PosY = gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        NegY = gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        PosZ = gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        NegZ = gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

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

    /// @brief Allocates and uploads a full mip level 0 image (2D targets).
    void setImage2D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                    gl::GLenum format, gl::GLenum type, const void* pixels) const;
    /// @brief Allocates and uploads a 3D texture / 2D array slice image.
    void setImage3D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                    gl::GLsizei depth, gl::GLenum format, gl::GLenum type, const void* pixels) const;
    /// @brief Uploads one cube map face (level 0 image of that face).
    void setImageCubeFace(CubeFace face, gl::GLint level, gl::GLint internalFormat, gl::GLsizei size,
                          gl::GLenum format, gl::GLenum type, const void* pixels) const;
    /// @brief Updates a sub-rectangle of an existing level.
    void setSubImage2D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLsizei width, gl::GLsizei height,
                       gl::GLenum format, gl::GLenum type, const void* pixels) const;
    /// @brief Immutable storage, mip chain included (GL 4.2; falls back to setImage2D with null data).
    void texStorage2D(gl::GLsizei levels, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height) const;
    /// @brief Immutable 3D/array storage (GL 4.2).
    void texStorage3D(gl::GLsizei levels, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height, gl::GLsizei depth) const;
    /// @brief Multisample storage (GL 3.3+; no-op if unsupported).
    void storageMultisample2D(gl::GLsizei samples, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height, bool fixedLocations = true) const;
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
