#pragma once

/// @file program_pipeline.hpp
/// @brief ProgramPipeline: GL 4.1 separable program objects (glCreateProgramPipelines,
/// glUseProgramStages, glProgramUniform*, glCreateShaderProgramv) — handle wrapper.

#include "types.hpp"
#include "../math.hpp"

#include <string>

namespace fuhgl {

/// @brief A GL program pipeline object (GL 4.1): stages reference separable programs.
///
/// Typical use: build one separable program per stage (Program::setParameteri
/// GL_PROGRAM_SEPARABLE), create the pipeline, useStages(), then render with use().
class ProgramPipeline {
public:
    ProgramPipeline();
    ~ProgramPipeline();

    ProgramPipeline(const ProgramPipeline&) = delete;
    ProgramPipeline& operator=(const ProgramPipeline&) = delete;
    ProgramPipeline(ProgramPipeline&&) noexcept;
    ProgramPipeline& operator=(ProgramPipeline&&) noexcept;

    /// @brief Creates the pipeline object (GL 4.1+).
    bool create();
    /// @brief Makes this the current pipeline (replaces glUseProgram for pipeline rendering).
    void use() const;
    /// @brief Binds `program` to the given shader stage bits of this pipeline.
    void useStages(gl::GLbitfield stages, gl::GLuint program) const;
    /// @brief Sets the active program of the pipeline (for glProgramUniform* targeting).
    void setActiveShaderProgram(gl::GLuint program) const;
    /// @brief Validates the pipeline; check infoLog() on failure.
    bool validate() const;
    std::string infoLog() const;
    void getiv(gl::GLenum pname, gl::GLint* params) const;

    // --- program uniforms: glProgramUniform*(program, ...) targets a separable program ---
    void programUniform(gl::GLuint program, gl::GLint location, int v) const;
    void programUniform(gl::GLuint program, gl::GLint location, float v) const;
    void programUniform(gl::GLuint program, gl::GLint location, unsigned v) const;
    void programUniform(gl::GLuint program, gl::GLint location, const math::Vec4& v) const;
    void programUniform(gl::GLuint program, gl::GLint location, const math::Mat4& v) const;

    /// @brief Compiles+links a single-stage separable program in one call (glCreateShaderProgramv, GL 4.1).
    /// @return program id (0 on failure).
    static gl::GLuint createShaderProgram(gl::GLenum shaderType, const char* source);

    gl::GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }

private:
    gl::GLuint id_ = 0;
};

} // namespace fuhgl
