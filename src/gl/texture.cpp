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
void Texture::generateMipmap() const { gl::functions().glGenerateMipmap(static_cast<gl::GLenum>(impl_->target)); }
void Texture::setParameteri(gl::GLenum pname, gl::GLint param) const {
    gl::functions().glTexParameteri(static_cast<gl::GLenum>(impl_->target), pname, param);
}
void Texture::setParameterf(gl::GLenum pname, gl::GLfloat param) const {
    gl::functions().glTexParameterf(static_cast<gl::GLenum>(impl_->target), pname, param);
}

gl::GLuint Texture::id() const { return impl_->id; }

} // namespace fuhgl
