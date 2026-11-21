#pragma once

/// @file program.hpp
/// @brief RAII wrapper around a linked GL program with a uniform-location cache.

#include "shader.hpp"
#include "types.hpp"
#include "../math/mat.hpp"
#include "../math/vec.hpp"

#include <memory>
#include <string>

namespace fuhgl {

/// @brief Linked GL program. attach() shaders, link(), then use() + setUniform().
class Program {
public:
    Program();
    ~Program();

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    Program(Program&&) noexcept;
    Program& operator=(Program&&) noexcept;

    /// @brief (Re)creates the program object.
    bool create();
    void attach(const Shader& shader);
    /// @brief Links the attached shaders. Returns false on error (see infoLog()).
    bool link();

    bool isLinked() const;
    std::string infoLog() const;

    /// @brief Makes this the current program.
    void use() const;

    /// @brief Uniform location with caching; returns -1 if the uniform does not exist/is optimized out.
    gl::GLint uniformLocation(const char* name) const;

    // --- typed uniform setters (location must belong to this program) ---
    void setUniform(gl::GLint location, int v) const;
    void setUniform(gl::GLint location, float v) const;
    void setUniform(gl::GLint location, const math::Vec2& v) const;
    void setUniform(gl::GLint location, const math::Vec3& v) const;
    void setUniform(gl::GLint location, const math::Vec4& v) const;
    void setUniform(gl::GLint location, const math::Mat3& v) const;
    void setUniform(gl::GLint location, const math::Mat4& v) const;
    void setUniform(gl::GLint location, gl::GLsizei count, const int* values) const;
    void setUniform(gl::GLint location, gl::GLuint v) const;
    void setUniform(gl::GLint location, gl::GLsizei count, const gl::GLuint* values) const;
    /// @brief Convenience overloads: look up the name and set the value.
    void setUniform(const char* name, const math::Mat4& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Mat3& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Vec4& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Vec3& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Vec2& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, float v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, int v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, unsigned v) const { setUniform(uniformLocation(name), v); }

    /// @brief Reads back a uniform value from the program object (glGetUniform*, no use() needed).
    gl::GLint getUniformi(const char* name) const;
    gl::GLfloat getUniformf(const char* name) const;
    gl::GLuint getUniformu(const char* name) const;

    /// @brief Binds a uniform block (by name) to an indexed binding point (e.g. 0).
    void bindUniformBlock(const char* blockName, gl::GLuint binding) const;
    /// @brief Binds a shader storage block (by name) to an indexed binding point (GL 4.3+, SSBO).
    void bindShaderStorageBlock(const char* blockName, gl::GLuint binding) const;

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
