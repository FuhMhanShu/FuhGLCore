#include "fuhgl/gl/program.hpp"
#include "fuhgl/gl/loader.hpp"

#include <unordered_map>

namespace fuhgl {

struct Program::Impl {
    gl::GLuint id = 0;
    bool linked = false;
    std::unordered_map<std::string, gl::GLint> uniformCache;
};

Program::Program() : impl_(std::make_unique<Impl>()) {}
Program::~Program() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteProgram)
        gl::functions().glDeleteProgram(impl_->id);
}
Program::Program(Program&&) noexcept = default;
Program& Program::operator=(Program&&) noexcept = default;

bool Program::create() {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteProgram)
        gl::functions().glDeleteProgram(impl_->id);
    impl_->id = 0;
    impl_->linked = false;
    impl_->uniformCache.clear();
    if (!gl::isInitialized() || !gl::functions().glCreateProgram) return false;
    impl_->id = gl::functions().glCreateProgram();
    return impl_->id != 0;
}

void Program::attach(const Shader& shader) {
    if (impl_->id && shader.id()) gl::functions().glAttachShader(impl_->id, shader.id());
}

bool Program::link() {
    if (!impl_->id) return false;
    const auto& f = gl::functions();
    f.glLinkProgram(impl_->id);
    gl::GLint status = 0;
    f.glGetProgramiv(impl_->id, gl::GL_LINK_STATUS, &status);
    impl_->linked = status == gl::GL_TRUE;
    return impl_->linked;
}

bool Program::isLinked() const { return impl_->linked; }

std::string Program::infoLog() const {
    if (!impl_->id) return {};
    const auto& f = gl::functions();
    gl::GLint len = 0;
    f.glGetProgramiv(impl_->id, gl::GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) return {};
    std::string log(static_cast<std::size_t>(len), '\0');
    gl::GLsizei written = 0;
    f.glGetProgramInfoLog(impl_->id, len, &written, log.data());
    log.resize(static_cast<std::size_t>(written));
    return log;
}

void Program::use() const {
    if (impl_->id) gl::functions().glUseProgram(impl_->id);
}

gl::GLint Program::uniformLocation(const char* name) const {
    auto it = impl_->uniformCache.find(name);
    if (it != impl_->uniformCache.end()) return it->second;
    gl::GLint loc = gl::functions().glGetUniformLocation(impl_->id, name);
    impl_->uniformCache.emplace(name, loc);
    return loc;
}

void Program::setUniform(gl::GLint loc, int v) const { gl::functions().glUniform1i(loc, v); }
void Program::setUniform(gl::GLint loc, float v) const { gl::functions().glUniform1f(loc, v); }
void Program::setUniform(gl::GLint loc, const math::Vec2& v) const { gl::functions().glUniform2f(loc, v.x, v.y); }
void Program::setUniform(gl::GLint loc, const math::Vec3& v) const { gl::functions().glUniform3f(loc, v.x, v.y, v.z); }
void Program::setUniform(gl::GLint loc, const math::Vec4& v) const { gl::functions().glUniform4f(loc, v.x, v.y, v.z, v.w); }
void Program::setUniform(gl::GLint loc, const math::Mat3& v) const {
    gl::functions().glUniformMatrix3fv(loc, 1, gl::GL_FALSE, v.data());
}
void Program::setUniform(gl::GLint loc, const math::Mat4& v) const {
    gl::functions().glUniformMatrix4fv(loc, 1, gl::GL_FALSE, v.data());
}
void Program::setUniform(gl::GLint loc, gl::GLsizei count, const int* values) const {
    gl::functions().glUniform1iv(loc, count, values);
}
void Program::setUniform(gl::GLint loc, gl::GLuint v) const {
    if (gl::functions().glUniform1ui) gl::functions().glUniform1ui(loc, v);
}
void Program::setUniform(gl::GLint loc, gl::GLsizei count, const gl::GLuint* values) const {
    if (gl::functions().glUniform1uiv) gl::functions().glUniform1uiv(loc, count, values);
}

gl::GLint Program::getUniformi(const char* name) const {
    gl::GLint v = 0;
    if (gl::functions().glGetUniformiv) gl::functions().glGetUniformiv(impl_->id, uniformLocation(name), &v);
    return v;
}
gl::GLfloat Program::getUniformf(const char* name) const {
    gl::GLfloat v = 0.f;
    if (gl::functions().glGetUniformfv) gl::functions().glGetUniformfv(impl_->id, uniformLocation(name), &v);
    return v;
}
gl::GLuint Program::getUniformu(const char* name) const {
    gl::GLuint v = 0;
    if (gl::functions().glGetUniformuiv) gl::functions().glGetUniformuiv(impl_->id, uniformLocation(name), &v);
    return v;
}

void Program::setTransformFeedbackVaryings(gl::GLsizei count, const gl::GLchar* const* varyings, gl::GLenum bufferMode) const {
    if (const auto& f = gl::functions(); f.glTransformFeedbackVaryings)
        f.glTransformFeedbackVaryings(impl_->id, count, varyings, bufferMode);
}

void Program::setParameteri(gl::GLenum pname, gl::GLint value) const {
    if (const auto& f = gl::functions(); f.glProgramParameteri) f.glProgramParameteri(impl_->id, pname, value);
}

void Program::bindUniformBlock(const char* blockName, gl::GLuint binding) const {
    const auto& f = gl::functions();
    if (!f.glGetUniformBlockIndex || !f.glUniformBlockBinding) return;
    const gl::GLuint idx = f.glGetUniformBlockIndex(impl_->id, blockName);
    if (idx != gl::GL_INVALID_INDEX) f.glUniformBlockBinding(impl_->id, idx, binding);
}

void Program::bindShaderStorageBlock(const char* blockName, gl::GLuint binding) const {
    const auto& f = gl::functions();
    if (!f.glGetShaderStorageBlockIndex || !f.glShaderStorageBlockBinding) return;
    const gl::GLuint idx = f.glGetShaderStorageBlockIndex(impl_->id, blockName);
    if (idx != gl::GL_INVALID_INDEX) f.glShaderStorageBlockBinding(impl_->id, idx, binding);
}

gl::GLuint Program::id() const { return impl_->id; }

} // namespace fuhgl
