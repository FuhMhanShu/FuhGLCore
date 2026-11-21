#include "fuhgl/gl/framebuffer.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Framebuffer::Impl {
    gl::GLuint id = 0;
    int width = 0, height = 0;
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
void Framebuffer::attachTexture2D(unsigned int colorIndex, const Texture& texture, gl::GLint level) const {
    gl::functions().glFramebufferTexture2D(gl::GL_FRAMEBUFFER, gl::GL_COLOR_ATTACHMENT0 + colorIndex,
                                           gl::GL_TEXTURE_2D, texture.id(), level);
}
void Framebuffer::attachRenderbuffer(Attachment attachment, const Renderbuffer& rb) const {
    gl::functions().glFramebufferRenderbuffer(gl::GL_FRAMEBUFFER, static_cast<gl::GLenum>(attachment),
                                              gl::GL_RENDERBUFFER, rb.id());
}

gl::GLenum Framebuffer::status() const {
    return gl::functions().glCheckFramebufferStatus(gl::GL_FRAMEBUFFER);
}

void Framebuffer::blitTo(Framebuffer& dst, gl::GLint srcW, gl::GLint srcH, gl::GLint dstW, gl::GLint dstH,
                         gl::GLbitfield mask, bool linearFilter) const {
    const auto& f = gl::functions();
    f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, impl_->id);
    f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, dst.impl_->id);
    f.glBlitFramebuffer(0, 0, srcW, srcH, 0, 0, dstW, dstH, mask,
                        linearFilter ? gl::GL_LINEAR : gl::GL_NEAREST);
    f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, 0);
    f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, 0);
}

void Framebuffer::blitToDefault(const Framebuffer& src, gl::GLint dstW, gl::GLint dstH,
                                gl::GLbitfield mask, bool linearFilter) {
    const auto& f = gl::functions();
    f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, src.impl_->id);
    f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, 0);
    const gl::GLint sw = src.impl_->width > 0 ? src.impl_->width : dstW;
    const gl::GLint sh = src.impl_->height > 0 ? src.impl_->height : dstH;
    f.glBlitFramebuffer(0, 0, sw, sh, 0, 0, dstW, dstH, mask,
                        linearFilter ? gl::GL_LINEAR : gl::GL_NEAREST);
    f.glBindFramebuffer(gl::GL_READ_FRAMEBUFFER, 0);
    f.glBindFramebuffer(gl::GL_DRAW_FRAMEBUFFER, 0);
}

gl::GLuint Framebuffer::id() const { return impl_->id; }
void Framebuffer::setSize(int w, int h) { impl_->width = w; impl_->height = h; }
int Framebuffer::width() const { return impl_->width; }
int Framebuffer::height() const { return impl_->height; }

} // namespace fuhgl
