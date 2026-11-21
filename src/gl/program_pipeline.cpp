// ProgramPipeline wrapper (GL 4.1 separable programs).

#include "fuhgl/gl/program_pipeline.hpp"

#include "fuhgl/gl/loader.hpp"

#include <string>
#include <vector>

namespace fuhgl {

ProgramPipeline::ProgramPipeline() = default;
ProgramPipeline::~ProgramPipeline() {
    if (id_ && gl::functions().glDeleteProgramPipelines)
        gl::functions().glDeleteProgramPipelines(1, &id_);
}
ProgramPipeline::ProgramPipeline(ProgramPipeline&& o) noexcept : id_(o.id_) { o.id_ = 0; }
ProgramPipeline& ProgramPipeline::operator=(ProgramPipeline&& o) noexcept {
    if (this != &o) {
        if (id_ && gl::functions().glDeleteProgramPipelines)
            gl::functions().glDeleteProgramPipelines(1, &id_);
        id_ = o.id_;
        o.id_ = 0;
    }
    return *this;
}

bool ProgramPipeline::create() {
    if (id_) return true;
    const auto& f = gl::functions();
    if (!f.glCreateProgramPipelines) return false;
    f.glCreateProgramPipelines(1, &id_);
    return id_ != 0;
}

void ProgramPipeline::use() const {
    if (const auto& f = gl::functions(); f.glBindProgramPipeline) f.glBindProgramPipeline(id_);
}

void ProgramPipeline::useStages(gl::GLbitfield stages, gl::GLuint program) const {
    if (const auto& f = gl::functions(); f.glUseProgramStages) f.glUseProgramStages(id_, stages, program);
}

void ProgramPipeline::setActiveShaderProgram(gl::GLuint program) const {
    if (const auto& f = gl::functions(); f.glActiveShaderProgram) f.glActiveShaderProgram(id_, program);
}

bool ProgramPipeline::validate() const {
    const auto& f = gl::functions();
    if (!f.glValidateProgramPipeline) return false;
    f.glValidateProgramPipeline(id_);
    gl::GLint ok = 0;
    if (f.glGetProgramPipelineiv) f.glGetProgramPipelineiv(id_, gl::GL_VALIDATE_STATUS, &ok);
    return ok == gl::GL_TRUE;
}

std::string ProgramPipeline::infoLog() const {
    const auto& f = gl::functions();
    if (!f.glGetProgramPipelineInfoLog) return {};
    gl::GLint len = 0;
    f.glGetProgramPipelineiv(id_, gl::GL_INFO_LOG_LENGTH, &len);
    std::string s(static_cast<std::size_t>(len > 0 ? len : 1), '\0');
    gl::GLsizei written = 0;
    f.glGetProgramPipelineInfoLog(id_, static_cast<gl::GLsizei>(s.size()), &written, s.data());
    s.resize(static_cast<std::size_t>(written));
    return s;
}

void ProgramPipeline::getiv(gl::GLenum pname, gl::GLint* params) const {
    if (const auto& f = gl::functions(); f.glGetProgramPipelineiv) f.glGetProgramPipelineiv(id_, pname, params);
}

void ProgramPipeline::programUniform(gl::GLint location, int v) const {
    if (const auto& f = gl::functions(); f.glProgramUniform1i) f.glProgramUniform1i(id_, location, v);
}
void ProgramPipeline::programUniform(gl::GLint location, float v) const {
    if (const auto& f = gl::functions(); f.glProgramUniform1f) f.glProgramUniform1f(id_, location, v);
}
void ProgramPipeline::programUniform(gl::GLint location, unsigned v) const {
    if (const auto& f = gl::functions(); f.glProgramUniform1ui) f.glProgramUniform1ui(id_, location, v);
}
void ProgramPipeline::programUniform(gl::GLint location, const math::Vec4& v) const {
    if (const auto& f = gl::functions(); f.glProgramUniform4f) f.glProgramUniform4f(id_, location, v.x, v.y, v.z, v.w);
}
void ProgramPipeline::programUniform(gl::GLint location, const math::Mat4& v) const {
    if (const auto& f = gl::functions(); f.glProgramUniformMatrix4fv)
        f.glProgramUniformMatrix4fv(id_, location, 1, gl::GL_FALSE, v.data());
}

gl::GLuint ProgramPipeline::createShaderProgram(gl::GLenum shaderType, const char* source) {
    const auto& f = gl::functions();
    if (!f.glCreateShaderProgramv || !source) return 0;
    const gl::GLchar* src = source;
    return f.glCreateShaderProgramv(shaderType, 1, &src);
}

} // namespace fuhgl
