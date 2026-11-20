#include "fuhgl/gl/buffer.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Buffer::Impl {
    gl::GLuint id = 0;
    Target target = Target::Array;
};

Buffer::Buffer() : impl_(std::make_unique<Impl>()) {}
Buffer::Buffer(Target target) : impl_(std::make_unique<Impl>()) { create(target); }
Buffer::~Buffer() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteBuffers)
        gl::functions().glDeleteBuffers(1, &impl_->id);
}
Buffer::Buffer(Buffer&&) noexcept = default;
Buffer& Buffer::operator=(Buffer&&) noexcept = default;

bool Buffer::create(Target target) {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteBuffers)
        gl::functions().glDeleteBuffers(1, &impl_->id);
    impl_->id = 0;
    if (!gl::isInitialized() || !gl::functions().glGenBuffers) return false;
    gl::functions().glGenBuffers(1, &impl_->id);
    impl_->target = target;
    return impl_->id != 0;
}

void Buffer::bind() const { gl::functions().glBindBuffer(static_cast<gl::GLenum>(impl_->target), impl_->id); }
void Buffer::unbind(Target target) { gl::functions().glBindBuffer(static_cast<gl::GLenum>(target), 0); }

void Buffer::setData(gl::GLsizeiptr size, const void* data, Usage usage) const {
    gl::functions().glBufferData(static_cast<gl::GLenum>(impl_->target), size, data, static_cast<gl::GLenum>(usage));
}
void Buffer::setSubData(gl::GLintptr offset, gl::GLsizeiptr size, const void* data) const {
    gl::functions().glBufferSubData(static_cast<gl::GLenum>(impl_->target), offset, size, data);
}
void* Buffer::map(Access access) const {
    return gl::functions().glMapBuffer(static_cast<gl::GLenum>(impl_->target), static_cast<gl::GLenum>(access));
}
bool Buffer::unmap() const {
    return gl::functions().glUnmapBuffer(static_cast<gl::GLenum>(impl_->target)) == gl::GL_TRUE;
}
void Buffer::bindBase(gl::GLuint index) const {
    gl::functions().glBindBufferBase(static_cast<gl::GLenum>(impl_->target), index, impl_->id);
}
void Buffer::bindRange(gl::GLuint index, gl::GLintptr offset, gl::GLsizeiptr size) const {
    gl::functions().glBindBufferRange(static_cast<gl::GLenum>(impl_->target), index, impl_->id, offset, size);
}

gl::GLuint Buffer::id() const { return impl_->id; }

} // namespace fuhgl
