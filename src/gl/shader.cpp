#include "fuhgl/gl/shader.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl {

struct Shader::Impl {
    gl::GLuint id = 0;
    Type type = Type::Vertex;
    bool compiled = false;
};

Shader::Shader() : impl_(std::make_unique<Impl>()) {}
Shader::Shader(Type type) : impl_(std::make_unique<Impl>()) { create(type); }
Shader::~Shader() {
    if (impl_ && impl_->id && gl::isInitialized() && gl::functions().glDeleteShader)
        gl::functions().glDeleteShader(impl_->id);
}
Shader::Shader(Shader&&) noexcept = default;
Shader& Shader::operator=(Shader&&) noexcept = default;

bool Shader::create(Type type) {
    if (impl_->id && gl::isInitialized() && gl::functions().glDeleteShader)
        gl::functions().glDeleteShader(impl_->id);
    impl_->id = 0;
    impl_->compiled = false;
    if (!gl::isInitialized() || !gl::functions().glCreateShader) return false;
    impl_->id = gl::functions().glCreateShader(static_cast<gl::GLenum>(type));
    impl_->type = type;
    return impl_->id != 0;
}

bool Shader::compile(const char* source) {
    return compile(&source, 1, nullptr);
}

bool Shader::compile(const char* const* sources, gl::GLsizei count, const gl::GLint* lengths) {
    if (!impl_->id) return false;
    const auto& f = gl::functions();
    f.glShaderSource(impl_->id, count, sources, lengths);
    f.glCompileShader(impl_->id);
    gl::GLint status = 0;
    f.glGetShaderiv(impl_->id, gl::GL_COMPILE_STATUS, &status);
    impl_->compiled = status == gl::GL_TRUE;
    return impl_->compiled;
}

bool Shader::isCompiled() const { return impl_->compiled; }

std::string Shader::infoLog() const {
    if (!impl_->id) return {};
    const auto& f = gl::functions();
    gl::GLint len = 0;
    f.glGetShaderiv(impl_->id, gl::GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) return {};
    std::string log(static_cast<std::size_t>(len), '\0');
    gl::GLsizei written = 0;
    f.glGetShaderInfoLog(impl_->id, len, &written, log.data());
    log.resize(static_cast<std::size_t>(written));
    return log;
}

gl::GLuint Shader::id() const { return impl_->id; }

} // namespace fuhgl
