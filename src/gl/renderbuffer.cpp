#include "fuhgl/gl/renderbuffer.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Renderbuffer::Impl {
    gl::GLuint id = 0;
};

Renderbuffer::Renderbuffer() : impl_(std::make_unique<Impl>()) {}
Renderbuffer::~Renderbuffer() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteRenderbuffers)
        gl::functions().glDeleteRenderbuffers(1, &impl_->id);
}
Renderbuffer::Renderbuffer(Renderbuffer&&) noexcept = default;
Renderbuffer& Renderbuffer::operator=(Renderbuffer&&) noexcept = default;

bool Renderbuffer::create() {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteRenderbuffers)
        gl::functions().glDeleteRenderbuffers(1, &impl_->id);
    impl_->id = 0;
    if (!gl::isInitialized() || !gl::functions().glGenRenderbuffers) return false;
    gl::functions().glGenRenderbuffers(1, &impl_->id);
    return impl_->id != 0;
}

void Renderbuffer::bind() const { gl::functions().glBindRenderbuffer(gl::GL_RENDERBUFFER, impl_->id); }
void Renderbuffer::unbind() { gl::functions().glBindRenderbuffer(gl::GL_RENDERBUFFER, 0); }
void Renderbuffer::storage(gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height) const {
    gl::functions().glRenderbufferStorage(gl::GL_RENDERBUFFER, internalFormat, width, height);
}
void Renderbuffer::storageMultisample(gl::GLsizei samples, gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height) const {
    if (gl::functions().glRenderbufferStorageMultisample)
        gl::functions().glRenderbufferStorageMultisample(gl::GL_RENDERBUFFER, samples, internalFormat, width, height);
}

gl::GLuint Renderbuffer::id() const { return impl_->id; }

} // namespace fuhgl
