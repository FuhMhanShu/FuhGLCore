#pragma once

/// @file shader.hpp
/// @brief RAII wrapper around a GL shader object (PIMPL, movable, non-copyable).

#include "types.hpp"

#include <functional>
#include <memory>
#include <string>

namespace fuhgl {

/// @brief Compiled GL shader stage. Create, compile(), check isCompiled()/infoLog().
class Shader {
public:
    /// @brief Shader stage.
    enum class Type : gl::GLenum {
        Vertex = gl::GL_VERTEX_SHADER,
        Fragment = gl::GL_FRAGMENT_SHADER,
        Geometry = gl::GL_GEOMETRY_SHADER,
        Compute = gl::GL_COMPUTE_SHADER,
    };

    Shader();
    /// @brief Creates the shader object immediately with the given stage.
    explicit Shader(Type type);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) noexcept;
    Shader& operator=(Shader&&) noexcept;

    /// @brief (Re)creates the shader object with the given stage. Destroys any previous object.
    bool create(Type type);
    /// @brief Compiles a single null-terminated GLSL source string (nullptr is treated as empty).
    /// Returns false on error (see infoLog()).
    bool compile(const char* source);
    /// @brief Compiles multiple source strings concatenated by the GL.
    bool compile(const char* const* sources, gl::GLsizei count, const gl::GLint* lengths = nullptr);

    /// @brief Resolves `#include "path"` lines in a GLSL source via the given resolver.
    /// @param resolver maps an include path to file contents (may return empty on failure).
    /// @param depth internal recursion guard; the result keeps `#version` as the first line.
    /// @note Hardened: include depth is limited to 16 levels and the resolved output is
    /// capped at 4 MiB; beyond either limit the remaining source passes through unresolved
    /// (which then fails the GL compile with an info log instead of exhausting memory).
    using IncludeResolver = std::function<std::string(const std::string& path)>;
    static std::string resolveIncludes(const std::string& source, const IncludeResolver& resolver, int depth = 0);
    /// @brief resolveIncludes() + compile() in one call.
    bool compileWithIncludes(const char* source, const IncludeResolver& resolver);

    bool isCompiled() const;
    /// @brief Info log of the last compile (empty if none).
    std::string infoLog() const;

    /// @brief Raw GL shader name, 0 if not created.
    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
