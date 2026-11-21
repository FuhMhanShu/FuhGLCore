// TransformFeedback wrapper (GL 4.0).

#include "fuhgl/gl/transform_feedback.hpp"

#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

TransformFeedback::TransformFeedback() = default;
TransformFeedback::~TransformFeedback() {
    if (id_ && gl::functions().glDeleteTransformFeedbacks)
        gl::functions().glDeleteTransformFeedbacks(1, &id_);
}
TransformFeedback::TransformFeedback(TransformFeedback&& o) noexcept : id_(o.id_) { o.id_ = 0; }
TransformFeedback& TransformFeedback::operator=(TransformFeedback&& o) noexcept {
    if (this != &o) {
        if (id_ && gl::functions().glDeleteTransformFeedbacks)
            gl::functions().glDeleteTransformFeedbacks(1, &id_);
        id_ = o.id_;
        o.id_ = 0;
    }
    return *this;
}

bool TransformFeedback::create() {
    if (id_) return true;
    const auto& f = gl::functions();
    if (!f.glGenTransformFeedbacks) return false;
    f.glGenTransformFeedbacks(1, &id_);
    return id_ != 0;
}

void TransformFeedback::bind() const {
    if (const auto& f = gl::functions(); f.glBindTransformFeedback) f.glBindTransformFeedback(gl::GL_TRANSFORM_FEEDBACK, id_);
}

void TransformFeedback::begin(gl::GLenum primitiveMode) const {
    if (const auto& f = gl::functions(); f.glBeginTransformFeedback) f.glBeginTransformFeedback(primitiveMode);
}
void TransformFeedback::end() const {
    if (const auto& f = gl::functions(); f.glEndTransformFeedback) f.glEndTransformFeedback();
}
void TransformFeedback::pause() const {
    if (const auto& f = gl::functions(); f.glPauseTransformFeedback) f.glPauseTransformFeedback();
}
void TransformFeedback::resume() const {
    if (const auto& f = gl::functions(); f.glResumeTransformFeedback) f.glResumeTransformFeedback();
}
void TransformFeedback::draw(gl::GLenum mode) const {
    if (const auto& f = gl::functions(); f.glDrawTransformFeedback) f.glDrawTransformFeedback(mode, id_);
}

void TransformFeedback::geti_v(gl::GLenum pname, gl::GLuint index, gl::GLint* param) const {
    if (const auto& f = gl::functions(); f.glGetTransformFeedbacki_v) f.glGetTransformFeedbacki_v(id_, pname, index, param);
}

} // namespace fuhgl
