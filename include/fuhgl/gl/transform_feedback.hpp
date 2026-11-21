#pragma once

/// @file transform_feedback.hpp
/// @brief TransformFeedback: GL 4.0 transform feedback object wrapper
/// (glGen/Bind/Begin/End/Pause/ResumeTransformFeedback, glDrawTransformFeedback).
/// Varying capture is configured on the program via Program::setTransformFeedbackVaryings().

#include "types.hpp"

namespace fuhgl {

/// @brief A GL transform feedback object: captures vertex shader outputs into buffers.
class TransformFeedback {
public:
    TransformFeedback();
    ~TransformFeedback();

    TransformFeedback(const TransformFeedback&) = delete;
    TransformFeedback& operator=(const TransformFeedback&) = delete;
    TransformFeedback(TransformFeedback&&) noexcept;
    TransformFeedback& operator=(TransformFeedback&&) noexcept;

    /// @brief Creates the object (GL 4.0+).
    bool create();
    void bind() const;

    /// @brief Starts capture of the given primitive mode (GL_POINTS/LINES/TRIANGLES).
    void begin(gl::GLenum primitiveMode) const;
    void end() const;
    void pause() const;
    void resume() const;
    /// @brief Replays a captured vertex stream (draw without running the VS again).
    void draw(gl::GLenum mode) const;

    /// @brief Queries a TF state (e.g. GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN on the bound object).
    void geti_v(gl::GLenum pname, gl::GLuint index, gl::GLint* param) const;

    gl::GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }

private:
    gl::GLuint id_ = 0;
};

} // namespace fuhgl
