#include "fuhgl/gl/vao.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct VertexArray::Impl {
    gl::GLuint id = 0;
};

VertexArray::VertexArray() : impl_(std::make_unique<Impl>()) {}
VertexArray::~VertexArray() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteVertexArrays)
        gl::functions().glDeleteVertexArrays(1, &impl_->id);
}
VertexArray::VertexArray(VertexArray&&) noexcept = default;
VertexArray& VertexArray::operator=(VertexArray&&) noexcept = default;

bool VertexArray::create() {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteVertexArrays)
        gl::functions().glDeleteVertexArrays(1, &impl_->id);
    impl_->id = 0;
    if (!gl::isInitialized() || !gl::functions().glGenVertexArrays) return false;
    gl::functions().glGenVertexArrays(1, &impl_->id);
    return impl_->id != 0;
}

void VertexArray::bind() const { gl::functions().glBindVertexArray(impl_->id); }
void VertexArray::unbind() { gl::functions().glBindVertexArray(0); }

void VertexArray::setAttrib(gl::GLuint index, gl::GLint size, gl::GLenum type, bool normalized,
                            gl::GLsizei stride, gl::GLintptr offset) {
    const auto& f = gl::functions();
    f.glEnableVertexAttribArray(index);
    f.glVertexAttribPointer(index, size, type, normalized ? gl::GL_TRUE : gl::GL_FALSE, stride,
                            reinterpret_cast<const void*>(offset));
}
void VertexArray::setAttribI(gl::GLuint index, gl::GLint size, gl::GLenum type,
                             gl::GLsizei stride, gl::GLintptr offset) {
    const auto& f = gl::functions();
    if (!f.glVertexAttribIPointer) return;
    f.glEnableVertexAttribArray(index);
    f.glVertexAttribIPointer(index, size, type, stride, reinterpret_cast<const void*>(offset));
}
void VertexArray::setAttribL(gl::GLuint index, gl::GLint size, gl::GLenum type,
                             gl::GLsizei stride, gl::GLintptr offset) {
    const auto& f = gl::functions();
    if (!f.glVertexAttribLPointer) return;
    f.glEnableVertexAttribArray(index);
    f.glVertexAttribLPointer(index, size, type, stride, reinterpret_cast<const void*>(offset));
}
void VertexArray::enableAttrib(gl::GLuint index) { gl::functions().glEnableVertexAttribArray(index); }
void VertexArray::disableAttrib(gl::GLuint index) { gl::functions().glDisableVertexAttribArray(index); }
void VertexArray::setDivisor(gl::GLuint index, gl::GLuint divisor) { gl::functions().glVertexAttribDivisor(index, divisor); }
void VertexArray::setElementBuffer(const Buffer& buffer) {
    gl::functions().glBindBuffer(gl::GL_ELEMENT_ARRAY_BUFFER, buffer.id());
}

void VertexArray::bindVertexBuffer(gl::GLuint bindingIndex, const Buffer& buffer, gl::GLintptr offset, gl::GLsizei stride) {
    if (gl::functions().glBindVertexBuffer)
        gl::functions().glBindVertexBuffer(bindingIndex, buffer.id(), offset, stride);
}
void VertexArray::bindVertexBuffers(gl::GLuint first, gl::GLsizei count, const gl::GLuint* buffers,
                                    const gl::GLintptr* offsets, const gl::GLsizei* strides) {
    if (gl::functions().glBindVertexBuffers)
        gl::functions().glBindVertexBuffers(first, count, buffers, offsets, strides);
}
void VertexArray::setAttribBinding(gl::GLuint attribIndex, gl::GLuint bindingIndex) {
    if (gl::functions().glVertexAttribBinding) gl::functions().glVertexAttribBinding(attribIndex, bindingIndex);
}
void VertexArray::setAttribFormat(gl::GLuint attribIndex, gl::GLint size, gl::GLenum type, bool normalized, gl::GLuint relativeOffset) {
    if (gl::functions().glVertexAttribFormat)
        gl::functions().glVertexAttribFormat(attribIndex, size, type, normalized ? gl::GL_TRUE : gl::GL_FALSE, relativeOffset);
}
void VertexArray::setAttribIFormat(gl::GLuint attribIndex, gl::GLint size, gl::GLenum type, gl::GLuint relativeOffset) {
    if (gl::functions().glVertexAttribIFormat) gl::functions().glVertexAttribIFormat(attribIndex, size, type, relativeOffset);
}
void VertexArray::setAttribLFormat(gl::GLuint attribIndex, gl::GLint size, gl::GLenum type, gl::GLuint relativeOffset) {
    if (gl::functions().glVertexAttribLFormat) gl::functions().glVertexAttribLFormat(attribIndex, size, type, relativeOffset);
}
void VertexArray::setBindingDivisor(gl::GLuint bindingIndex, gl::GLuint divisor) {
    if (gl::functions().glVertexBindingDivisor) gl::functions().glVertexBindingDivisor(bindingIndex, divisor);
}

gl::GLuint VertexArray::id() const { return impl_->id; }

} // namespace fuhgl
