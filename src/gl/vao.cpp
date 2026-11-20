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
void VertexArray::enableAttrib(gl::GLuint index) { gl::functions().glEnableVertexAttribArray(index); }
void VertexArray::disableAttrib(gl::GLuint index) { gl::functions().glDisableVertexAttribArray(index); }
void VertexArray::setDivisor(gl::GLuint index, gl::GLuint divisor) { gl::functions().glVertexAttribDivisor(index, divisor); }
void VertexArray::setElementBuffer(const Buffer& buffer) {
    gl::functions().glBindBuffer(gl::GL_ELEMENT_ARRAY_BUFFER, buffer.id());
}

gl::GLuint VertexArray::id() const { return impl_->id; }

} // namespace fuhgl
