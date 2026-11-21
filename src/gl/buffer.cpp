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
void* Buffer::mapRange(gl::GLintptr offset, gl::GLsizeiptr length, gl::GLbitfield access) const {
    if (!gl::functions().glMapBufferRange) return nullptr;
    return gl::functions().glMapBufferRange(static_cast<gl::GLenum>(impl_->target), offset, length, access);
}
void Buffer::flushMappedRange(gl::GLintptr offset, gl::GLsizeiptr length) const {
    if (gl::functions().glFlushMappedBufferRange)
        gl::functions().glFlushMappedBufferRange(static_cast<gl::GLenum>(impl_->target), offset, length);
}
bool Buffer::unmap() const {
    return gl::functions().glUnmapBuffer(static_cast<gl::GLenum>(impl_->target)) == gl::GL_TRUE;
}
void Buffer::invalidateData() const {
    if (gl::functions().glInvalidateBufferData) gl::functions().glInvalidateBufferData(impl_->id);
}
void Buffer::invalidateSubData(gl::GLintptr offset, gl::GLsizeiptr length) const {
    if (gl::functions().glInvalidateBufferSubData) gl::functions().glInvalidateBufferSubData(impl_->id, offset, length);
}
void Buffer::getSubData(gl::GLintptr offset, gl::GLsizeiptr size, void* data) const {
    if (!gl::functions().glGetBufferSubData) return;
    bind();
    gl::functions().glGetBufferSubData(static_cast<gl::GLenum>(impl_->target), offset, size, data);
}
gl::GLint Buffer::parameteri(gl::GLenum pname) const {
    if (!gl::functions().glGetBufferParameteriv) return 0;
    bind();
    gl::GLint v = 0;
    gl::functions().glGetBufferParameteriv(static_cast<gl::GLenum>(impl_->target), pname, &v);
    return v;
}
gl::GLint64 Buffer::parameteri64(gl::GLenum pname) const {
    if (gl::functions().glGetBufferParameteri64v) {
        bind();
        gl::GLint64 v = 0;
        gl::functions().glGetBufferParameteri64v(static_cast<gl::GLenum>(impl_->target), pname, &v);
        return v;
    }
    return parameteri(pname);
}
void Buffer::bindBase(gl::GLuint index) const {
    gl::functions().glBindBufferBase(static_cast<gl::GLenum>(impl_->target), index, impl_->id);
}
void Buffer::bindRange(gl::GLuint index, gl::GLintptr offset, gl::GLsizeiptr size) const {
    gl::functions().glBindBufferRange(static_cast<gl::GLenum>(impl_->target), index, impl_->id, offset, size);
}

gl::GLuint Buffer::id() const { return impl_->id; }

} // namespace fuhgl
