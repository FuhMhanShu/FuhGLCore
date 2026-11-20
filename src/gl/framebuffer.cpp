#include "fuhgl/gl/framebuffer.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Framebuffer::Impl {
    gl::GLuint id = 0;
};

Framebuffer::Framebuffer() : impl_(std::make_unique<Impl>()) {}
Framebuffer::~Framebuffer() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteFramebuffers)
        gl::functions().glDeleteFramebuffers(1, &impl_->id);
}
Framebuffer::Framebuffer(Framebuffer&&) noexcept = default;
Framebuffer& Framebuffer::operator=(Framebuffer&&) noexcept = default;

bool Framebuffer::create() {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteFramebuffers)
        gl::functions().glDeleteFramebuffers(1, &impl_->id);
    impl_->id = 0;
    if (!gl::isInitialized() || !gl::functions().glGenFramebuffers) return false;
    gl::functions().glGenFramebuffers(1, &impl_->id);
    return impl_->id != 0;
}

void Framebuffer::bind(Target target) const {
    gl::functions().glBindFramebuffer(static_cast<gl::GLenum>(target), impl_->id);
}
void Framebuffer::unbind(Target target) {
    gl::functions().glBindFramebuffer(static_cast<gl::GLenum>(target), 0);
}

void Framebuffer::attachTexture2D(Attachment attachment, const Texture& texture, gl::GLint level) const {
    gl::functions().glFramebufferTexture2D(gl::GL_FRAMEBUFFER, static_cast<gl::GLenum>(attachment),
                                           gl::GL_TEXTURE_2D, texture.id(), level);
}
void Framebuffer::attachRenderbuffer(Attachment attachment, const Renderbuffer& rb) const {
    gl::functions().glFramebufferRenderbuffer(gl::GL_FRAMEBUFFER, static_cast<gl::GLenum>(attachment),
                                              gl::GL_RENDERBUFFER, rb.id());
}

gl::GLenum Framebuffer::status() const {
    return gl::functions().glCheckFramebufferStatus(gl::GL_FRAMEBUFFER);
}

gl::GLuint Framebuffer::id() const { return impl_->id; }

} // namespace fuhgl
