#include "fuhgl/gl/texture.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Texture::Impl {
    gl::GLuint id = 0;
    Target target = Target::Tex2D;
};

Texture::Texture() : impl_(std::make_unique<Impl>()) {}
Texture::Texture(Target target) : impl_(std::make_unique<Impl>()) { create(target); }
Texture::~Texture() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteTextures)
        gl::functions().glDeleteTextures(1, &impl_->id);
}
Texture::Texture(Texture&&) noexcept = default;
Texture& Texture::operator=(Texture&&) noexcept = default;

bool Texture::create(Target target) {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteTextures)
        gl::functions().glDeleteTextures(1, &impl_->id);
    impl_->id = 0;
    if (!gl::isInitialized() || !gl::functions().glGenTextures) return false;
    gl::functions().glGenTextures(1, &impl_->id);
    impl_->target = target;
    return impl_->id != 0;
}

void Texture::bind(gl::GLenum unit) const {
    const auto& f = gl::functions();
    f.glActiveTexture(unit);
    f.glBindTexture(static_cast<gl::GLenum>(impl_->target), impl_->id);
}
void Texture::unbind(Target target) { gl::functions().glBindTexture(static_cast<gl::GLenum>(target), 0); }

void Texture::setImage2D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                         gl::GLenum format, gl::GLenum type, const void* pixels) const {
    gl::functions().glTexImage2D(static_cast<gl::GLenum>(impl_->target), level, internalFormat, width, height,
                                 0, format, type, pixels);
}
void Texture::setImage3D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                         gl::GLsizei depth, gl::GLenum format, gl::GLenum type, const void* pixels) const {
    using PFN = void (*)(gl::GLenum, gl::GLint, gl::GLint, gl::GLsizei, gl::GLsizei, gl::GLsizei, gl::GLint, gl::GLenum, gl::GLenum, const void*);
    if (auto p = reinterpret_cast<PFN>(gl::procAddress("glTexImage3D")))
        p(static_cast<gl::GLenum>(impl_->target), level, internalFormat, width, height, depth, 0, format, type, pixels);
}
void Texture::setImageCubeFace(CubeFace face, gl::GLint level, gl::GLint internalFormat, gl::GLsizei size,
                               gl::GLenum format, gl::GLenum type, const void* pixels) const {
    gl::functions().glTexImage2D(static_cast<gl::GLenum>(face), level, internalFormat, size, size, 0, format, type, pixels);
}
void Texture::setSubImage2D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLsizei width,
                            gl::GLsizei height, gl::GLenum format, gl::GLenum type, const void* pixels) const {
    gl::functions().glTexSubImage2D(static_cast<gl::GLenum>(impl_->target), level, xoffset, yoffset, width, height,
                                    format, type, pixels);
}
void Texture::texStorage2D(gl::GLsizei levels, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height) const {
    if (gl::functions().glTexStorage2D)
        gl::functions().glTexStorage2D(static_cast<gl::GLenum>(impl_->target), levels, internalFormat, width, height);
    else
        gl::functions().glTexImage2D(static_cast<gl::GLenum>(impl_->target), 0, internalFormat, width, height, 0,
                                     gl::GL_RGBA, gl::GL_UNSIGNED_BYTE, nullptr);
}
void Texture::texStorage3D(gl::GLsizei levels, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                           gl::GLsizei depth) const {
    if (gl::functions().glTexStorage3D)
        gl::functions().glTexStorage3D(static_cast<gl::GLenum>(impl_->target), levels, internalFormat, width, height, depth);
}
void Texture::storageMultisample2D(gl::GLsizei samples, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                                   bool fixedLocations) const {
    if (gl::functions().glTexStorage2DMultisample)
        gl::functions().glTexStorage2DMultisample(static_cast<gl::GLenum>(impl_->target), samples, internalFormat,
                                                  width, height, fixedLocations ? gl::GL_TRUE : gl::GL_FALSE);
}
void Texture::setSubImage3D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLint zoffset,
                            gl::GLsizei width, gl::GLsizei height, gl::GLsizei depth,
                            gl::GLenum format, gl::GLenum type, const void* pixels) const {
    if (gl::functions().glTexSubImage3D)
        gl::functions().glTexSubImage3D(static_cast<gl::GLenum>(impl_->target), level, xoffset, yoffset, zoffset,
                                        width, height, depth, format, type, pixels);
}
void Texture::copyTexImage2D(gl::GLint level, gl::GLint internalFormat, gl::GLint x, gl::GLint y,
                             gl::GLsizei width, gl::GLsizei height, gl::GLint border) const {
    if (gl::functions().glCopyTexImage2D)
        gl::functions().glCopyTexImage2D(static_cast<gl::GLenum>(impl_->target), level, internalFormat, x, y,
                                         width, height, border);
}
void Texture::copyTexSubImage2D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset,
                                gl::GLint x, gl::GLint y, gl::GLsizei width, gl::GLsizei height) const {
    if (gl::functions().glCopyTexSubImage2D)
        gl::functions().glCopyTexSubImage2D(static_cast<gl::GLenum>(impl_->target), level, xoffset, yoffset, x, y,
                                            width, height);
}
void Texture::copyTexSubImage3D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLint zoffset,
                                gl::GLint x, gl::GLint y, gl::GLsizei width, gl::GLsizei height) const {
    if (gl::functions().glCopyTexSubImage3D)
        gl::functions().glCopyTexSubImage3D(static_cast<gl::GLenum>(impl_->target), level, xoffset, yoffset, zoffset,
                                            x, y, width, height);
}
void Texture::compressedImage2D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                                gl::GLsizei imageSize, const void* data, gl::GLint border) const {
    if (gl::functions().glCompressedTexImage2D)
        gl::functions().glCompressedTexImage2D(static_cast<gl::GLenum>(impl_->target), level, internalFormat, width,
                                               height, border, imageSize, data);
}
void Texture::compressedImage3D(gl::GLint level, gl::GLint internalFormat, gl::GLsizei width, gl::GLsizei height,
                                gl::GLsizei depth, gl::GLsizei imageSize, const void* data, gl::GLint border) const {
    if (gl::functions().glCompressedTexImage3D)
        gl::functions().glCompressedTexImage3D(static_cast<gl::GLenum>(impl_->target), level, internalFormat, width,
                                               height, depth, border, imageSize, data);
}
void Texture::compressedSubImage2D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLsizei width,
                                   gl::GLsizei height, gl::GLenum format, gl::GLsizei imageSize, const void* data) const {
    if (gl::functions().glCompressedTexSubImage2D)
        gl::functions().glCompressedTexSubImage2D(static_cast<gl::GLenum>(impl_->target), level, xoffset, yoffset,
                                                  width, height, format, imageSize, data);
}
void Texture::compressedSubImage3D(gl::GLint level, gl::GLint xoffset, gl::GLint yoffset, gl::GLint zoffset,
                                   gl::GLsizei width, gl::GLsizei height, gl::GLsizei depth,
                                   gl::GLenum format, gl::GLsizei imageSize, const void* data) const {
    if (gl::functions().glCompressedTexSubImage3D)
        gl::functions().glCompressedTexSubImage3D(static_cast<gl::GLenum>(impl_->target), level, xoffset, yoffset,
                                                  zoffset, width, height, depth, format, imageSize, data);
}
void Texture::texBufferImage(gl::GLenum internalFormat, const Buffer& buffer) const {
    if (gl::functions().glTexBuffer)
        gl::functions().glTexBuffer(gl::GL_TEXTURE_BUFFER, internalFormat, buffer.id());
}
void Texture::texBufferRangeImage(gl::GLenum internalFormat, const Buffer& buffer, gl::GLintptr offset, gl::GLsizeiptr size) const {
    if (gl::functions().glTexBufferRange)
        gl::functions().glTexBufferRange(gl::GL_TEXTURE_BUFFER, internalFormat, buffer.id(), offset, size);
}
void Texture::view(Target target, const Texture& source, gl::GLenum internalFormat,
                   gl::GLuint minLevel, gl::GLuint numLevels, gl::GLuint minLayer, gl::GLuint numLayers) const {
    if (gl::functions().glTextureView)
        gl::functions().glTextureView(impl_->id, static_cast<gl::GLenum>(target), source.id(), internalFormat,
                                      minLevel, numLevels, minLayer, numLayers);
}
gl::GLint Texture::getParameteri(gl::GLenum pname) const {
    gl::GLint v = 0;
    if (gl::functions().glGetTexParameteriv)
        gl::functions().glGetTexParameteriv(static_cast<gl::GLenum>(impl_->target), pname, &v);
    return v;
}
gl::GLfloat Texture::getParameterf(gl::GLenum pname) const {
    gl::GLfloat v = 0.f;
    if (gl::functions().glGetTexParameterfv)
        gl::functions().glGetTexParameterfv(static_cast<gl::GLenum>(impl_->target), pname, &v);
    return v;
}
gl::GLint Texture::getLevelParameteri(gl::GLenum pname, gl::GLint level) const {
    gl::GLint v = 0;
    if (gl::functions().glGetTexLevelParameteriv)
        gl::functions().glGetTexLevelParameteriv(static_cast<gl::GLenum>(impl_->target), level, pname, &v);
    return v;
}

void Texture::generateMipmap() const { gl::functions().glGenerateMipmap(static_cast<gl::GLenum>(impl_->target)); }
void Texture::setParameteri(gl::GLenum pname, gl::GLint param) const {
    gl::functions().glTexParameteri(static_cast<gl::GLenum>(impl_->target), pname, param);
}
void Texture::setParameterf(gl::GLenum pname, gl::GLfloat param) const {
    gl::functions().glTexParameterf(static_cast<gl::GLenum>(impl_->target), pname, param);
}

gl::GLuint Texture::id() const { return impl_->id; }
Texture::Target Texture::target() const { return impl_->target; }

} // namespace fuhgl
