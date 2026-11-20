#pragma once

/// @file loader.hpp
/// @brief Runtime OpenGL function loader — zero dependencies.
///
/// Call fuhgl::gl::init() once after an OpenGL context is current
/// (fuhgl::Window does this automatically). The curated table below covers the
/// core functions used by the wrappers plus common entry points up to GL 4.6;
/// any other function (extensions, DSA, ...) is available via procAddress().

#include "types.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace fuhgl::gl {

/// @brief Parsed OpenGL version, e.g. {4, 6}.
struct Version {
    int major = 0;
    int minor = 0;
    /// @brief True if this version is at least (maj, min).
    constexpr bool atLeast(int maj, int min) const { return major > maj || (major == maj && minor >= min); }
    constexpr bool operator==(const Version& o) const { return major == o.major && minor == o.minor; }
    constexpr bool operator!=(const Version& o) const { return !(*this == o); }
};

/// @brief Curated table of core GL function pointers, filled by init().
struct Functions {
    // info / errors
    GLenum (*glGetError)();
    const GLubyte* (*glGetString)(GLenum name);
    const GLubyte* (*glGetStringi)(GLenum name, GLuint index);
    void (*glGetIntegerv)(GLenum pname, GLint* data);
    // state
    void (*glEnable)(GLenum cap);
    void (*glDisable)(GLenum cap);
    void (*glClear)(GLbitfield mask);
    void (*glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
    void (*glViewport)(GLint x, GLint y, GLsizei w, GLsizei h);
    void (*glScissor)(GLint x, GLint y, GLsizei w, GLsizei h);
    void (*glBlendFunc)(GLenum sfactor, GLenum dfactor);
    void (*glDepthFunc)(GLenum func);
    void (*glDepthMask)(GLboolean flag);
    void (*glColorMask)(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
    void (*glCullFace)(GLenum mode);
    void (*glFrontFace)(GLenum mode);
    void (*glPixelStorei)(GLenum pname, GLint param);
    void (*glFinish)();
    void (*glFlush)();
    // shaders
    GLuint (*glCreateShader)(GLenum type);
    void (*glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
    void (*glCompileShader)(GLuint shader);
    void (*glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
    void (*glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    void (*glDeleteShader)(GLuint shader);
    // programs
    GLuint (*glCreateProgram)();
    void (*glAttachShader)(GLuint program, GLuint shader);
    void (*glLinkProgram)(GLuint program);
    void (*glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
    void (*glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    void (*glUseProgram)(GLuint program);
    void (*glDeleteProgram)(GLuint program);
    GLint (*glGetUniformLocation)(GLuint program, const GLchar* name);
    GLint (*glGetAttribLocation)(GLuint program, const GLchar* name);
    void (*glBindAttribLocation)(GLuint program, GLuint index, const GLchar* name);
    void (*glUniform1i)(GLint location, GLint v0);
    void (*glUniform1f)(GLint location, GLfloat v0);
    void (*glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
    void (*glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void (*glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void (*glUniform1iv)(GLint location, GLsizei count, const GLint* value);
    void (*glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    // buffers
    void (*glGenBuffers)(GLsizei n, GLuint* buffers);
    void (*glBindBuffer)(GLenum target, GLuint buffer);
    void (*glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    void (*glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
    void (*glDeleteBuffers)(GLsizei n, const GLuint* buffers);
    void* (*glMapBuffer)(GLenum target, GLenum access);
    GLboolean (*glUnmapBuffer)(GLenum target);
    void (*glBindBufferBase)(GLenum target, GLuint index, GLuint buffer);
    void (*glBindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
    // vertex arrays
    void (*glGenVertexArrays)(GLsizei n, GLuint* arrays);
    void (*glBindVertexArray)(GLuint array);
    void (*glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
    void (*glEnableVertexAttribArray)(GLuint index);
    void (*glDisableVertexAttribArray)(GLuint index);
    void (*glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
    void (*glVertexAttribDivisor)(GLuint index, GLuint divisor);
    // textures
    void (*glGenTextures)(GLsizei n, GLuint* textures);
    void (*glBindTexture)(GLenum target, GLuint texture);
    void (*glDeleteTextures)(GLsizei n, const GLuint* textures);
    void (*glActiveTexture)(GLenum texture);
    void (*glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    void (*glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
    void (*glGenerateMipmap)(GLenum target);
    void (*glTexParameteri)(GLenum target, GLenum pname, GLint param);
    void (*glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
    // framebuffers / renderbuffers
    void (*glGenFramebuffers)(GLsizei n, GLuint* framebuffers);
    void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
    void (*glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
    void (*glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    GLenum (*glCheckFramebufferStatus)(GLenum target);
    void (*glDrawBuffers)(GLsizei n, const GLenum* bufs);
    void (*glBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
    void (*glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
    void (*glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
    void (*glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void (*glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    void (*glDeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
    // drawing
    void (*glDrawArrays)(GLenum mode, GLint first, GLsizei count);
    void (*glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
    void (*glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
    void (*glDrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
    // debug output (GL 4.3 / KHR_debug)
    void (*glDebugMessageCallback)(GLDEBUGPROC callback, const void* userParam);
};

/// @brief Load all functions in the curated table. Requires a *current* OpenGL context.
/// @return true if the table was filled (individual entries may still be null on old drivers).
bool init();

/// @brief True after a successful init().
bool isInitialized();

/// @brief Raw address lookup for any GL function by name (extensions, DSA, wgl/glX/egl, ...).
/// @return nullptr if not available on this platform/driver.
void* procAddress(const char* name);

/// @brief The curated function table (valid after init()).
const Functions& functions();

/// @brief Parsed GL_VERSION of the current context (call after init()).
Version version();

/// @brief Extension list of the current context (GL 4.3 style, with legacy fallback).
std::vector<std::string> extensions();

/// @brief True if the current context reports the given extension.
bool hasExtension(const char* name);

// --- pure helpers (unit-testable without a GL context) -------------------------

/// @brief Parse a GL_VERSION string like "4.6.0 NVIDIA 535.1" into {4, 6}.
Version parseVersion(const char* versionString);

/// @brief Split a legacy GL_EXTENSIONS string (space separated) into a list.
std::vector<std::string> splitExtensions(const char* extensionsString);

/// @brief Names of all functions in the curated table (for diagnostics/tests).
const char* const* functionNames();
/// @brief Number of entries in the curated table.
std::size_t functionNameCount();

} // namespace fuhgl::gl
