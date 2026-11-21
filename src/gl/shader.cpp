#include "fuhgl/gl/shader.hpp"
#include "fuhgl/gl/loader.hpp"

#include <sstream>

namespace fuhgl {

namespace {
std::string trimLeft(std::string s) {
    const auto pos = s.find_first_not_of(" \t\r");
    return pos == std::string::npos ? std::string{} : s.substr(pos);
}

// Hardening limits: include recursion depth and total resolved size, so a
// pathological include graph (self-including or exponentially expanding files
// behind an application-supplied resolver) cannot exhaust memory.
constexpr int kMaxIncludeDepth = 16;
constexpr std::size_t kMaxResolvedBytes = std::size_t{4} << 20; // 4 MiB

std::string resolveIncludesImpl(const std::string& source, const Shader::IncludeResolver& resolver,
                                int depth, std::size_t& budget) {
    if (depth > kMaxIncludeDepth || !resolver) return source;
    std::istringstream in(source);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line)) {
        const std::string t = trimLeft(line);
        if (t.rfind("#include", 0) == 0) {
            const auto q1 = t.find('"');
            const auto q2 = q1 == std::string::npos ? std::string::npos : t.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos && budget > 0) {
                const std::string path = t.substr(q1 + 1, q2 - q1 - 1);
                std::string content = resolver(path);
                if (content.size() > budget) content.resize(budget);
                budget -= content.size();
                out << resolveIncludesImpl(content, resolver, depth + 1, budget) << '\n';
                continue;
            }
        }
        out << line << '\n';
    }
    return out.str();
}
} // namespace

std::string Shader::resolveIncludes(const std::string& source, const IncludeResolver& resolver, int depth) {
    std::size_t budget = kMaxResolvedBytes;
    return resolveIncludesImpl(source, resolver, depth, budget);
}

bool Shader::compileWithIncludes(const char* source, const IncludeResolver& resolver) {
    return compile(resolveIncludes(source ? source : "", resolver).c_str());
}


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
    const char* safe = source ? source : ""; // never hand a null string to glShaderSource
    return compile(&safe, 1, nullptr);
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
