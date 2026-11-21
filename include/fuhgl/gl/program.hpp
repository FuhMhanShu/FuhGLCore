#pragma once

/// @file program.hpp
/// @brief RAII wrapper around a linked GL program with a uniform-location cache.

#include "shader.hpp"
#include "types.hpp"
#include "../math/mat.hpp"
#include "../math/vec.hpp"

#include <memory>
#include <string>
#include <vector>

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
    void setUniform(gl::GLint location, gl::GLsizei count, const float* values) const;
    void setUniform(gl::GLint location, gl::GLsizei count, const math::Mat4* values) const;
    /// @brief Convenience overloads: look up the name and set the value.
    void setUniform(const char* name, const math::Mat4& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Mat3& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Vec4& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Vec3& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, const math::Vec2& v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, float v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, int v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, unsigned v) const { setUniform(uniformLocation(name), v); }
    void setUniform(const char* name, gl::GLsizei count, const float* values) const { setUniform(uniformLocation(name), count, values); }
    void setUniform(const char* name, gl::GLsizei count, const math::Mat4* values) const { setUniform(uniformLocation(name), count, values); }

    /// @brief Reads back a uniform value from the program object (glGetUniform*, no use() needed).
    gl::GLint getUniformi(const char* name) const;
    gl::GLfloat getUniformf(const char* name) const;
    gl::GLuint getUniformu(const char* name) const;

    /// @brief Binds a uniform block (by name) to an indexed binding point (e.g. 0).
    /// @brief Declares transform feedback varyings before link() (GL 4.0, P3).
    void setTransformFeedbackVaryings(gl::GLsizei count, const gl::GLchar* const* varyings, gl::GLenum bufferMode) const;
    /// @brief Sets a program parameter (e.g. GL_PROGRAM_SEPARABLE, GL_PROGRAM_BINARY_RETRIEVABLE_HINT).
    void setParameteri(gl::GLenum pname, gl::GLint value) const;

    void bindUniformBlock(const char* blockName, gl::GLuint binding) const;
    /// @brief Binds a shader storage block (by name) to an indexed binding point (GL 4.3+, SSBO).
    void bindShaderStorageBlock(const char* blockName, gl::GLuint binding) const;

    // --- reflection (F22): query the linked program's interface (glGetActive*/Resource*) ---
    /// @brief One active uniform: name, type, size, block index (-1 = default block), offset, strides.
    struct UniformInfo {
        std::string name;
        gl::GLenum type = 0;
        gl::GLint size = 0;
        gl::GLint blockIndex = -1;
        gl::GLint offset = 0;
        gl::GLint arrayStride = 0;
    };
    /// @brief One active vertex attribute: name, type, size, location (glGetAttribLocation).
    struct AttributeInfo {
        std::string name;
        gl::GLenum type = 0;
        gl::GLint size = 0;
        gl::GLint location = -1;
    };
    /// @brief One active uniform block: name, index, data size, binding, active uniform indices.
    struct UniformBlockInfo {
        std::string name;
        gl::GLuint index = 0;
        gl::GLint dataSize = 0;
        gl::GLint binding = 0;
        gl::GLint activeUniforms = 0;
        std::vector<gl::GLuint> uniformIndices;
    };
    /// @brief All active uniforms of the linked program (empty if not linked / no reflection).
    std::vector<UniformInfo> activeUniforms() const;
    /// @brief All active vertex attributes of the linked program.
    std::vector<AttributeInfo> activeAttributes() const;
    /// @brief All active uniform blocks of the linked program.
    std::vector<UniformBlockInfo> uniformBlocks() const;

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
