#pragma once

/// @file texture.hpp
/// @brief RAII wrapper around a GL texture object (2D in v1).

#include "buffer.hpp"
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
        TexBuffer = gl::GL_TEXTURE_BUFFER,
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
    /// @brief Updates a sub-volume of an existing 3D/array level.
    void setSubImage3D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLint zoffset,
                       gl::GLsizei width, gl::GLsizei height, gl::GLsizei depth,
                       gl::GLenum format, gl::GLenum type, const void* pixels) const;
    /// @brief Copies a rectangle of the current framebuffer into this texture (render-to-texture).
    void copyTexImage2D(gl::GLint level, gl::GLint internalFormat, gl::GLint x, gl::GLint y,
                        gl::GLsizei width, gl::GLsizei height, gl::GLint border = 0) const;
    void copyTexSubImage2D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset,
                           gl::GLint x, gl::GLint y, gl::GLsizei width, gl::GLsizei height) const;
    void copyTexSubImage3D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLint zoffset,
                           gl::GLint x, gl::GLint y, gl::GLsizei width, gl::GLsizei height) const;
    /// @brief Uploads a compressed image (DXT/BC, ETC, ...) — imageSize = compressed byte count.
    void compressedImage2D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                           gl::GLsizei imageSize, const void* data, gl::GLint border = 0) const;
    void compressedImage3D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height, gl::GLsizei depth,
                           gl::GLsizei imageSize, const void* data, gl::GLint border = 0) const;
    void compressedSubImage2D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLsizei width, gl::GLsizei height,
                              gl::GLenum format, gl::GLsizei imageSize, const void* data) const;
    void compressedSubImage3D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLint zoffset,
                              gl::GLsizei width, gl::GLsizei height, gl::GLsizei depth,
                              gl::GLenum format, gl::GLsizei imageSize, const void* data) const;
    /// @brief Buffer texture: views a Buffer's storage as a 1D texture (GL 3.1+).
    void texBufferImage(gl::GLenum internalFormat, const Buffer& buffer) const;
    void texBufferRangeImage(gl::GLenum internalFormat, const Buffer& buffer, gl::GLintptr offset, gl::GLsizeiptr size) const;
    /// @brief Reinterprets another texture as this one (GL 4.3, zero-copy view).
    void view(Target target, const Texture& source, gl::GLenum internalFormat,
              gl::GLuint minLevel, gl::GLuint numLevels, gl::GLuint minLayer, gl::GLuint numLayers) const;
    /// @brief Queries a texture parameter of the bound texture (0 if unsupported).
    gl::GLint getParameteri(gl::GLenum pname) const;
    gl::GLfloat getParameterf(gl::GLenum pname) const;
    /// @brief Queries a level parameter (GL_TEXTURE_WIDTH/HEIGHT/COMPRESSED_IMAGE_SIZE/...).
    gl::GLint getLevelParameteri(gl::GLenum pname, gl::GLint level) const;
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
    Target target() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
