#pragma once

/// @file debug.hpp
/// @brief GL 4.3 / KHR_debug helpers beyond the callback (error.hpp): message
/// control/insert/poll, debug groups (push/pop) and object labels — e.g. for
/// RenderDoc / Nsight captures. All no-ops when the driver lacks KHR_debug.

#include "loader.hpp"
#include "types.hpp"

#include <string>

namespace fuhgl::gl {

/// @brief Object label identifier classes (glObjectLabel).
enum class ObjectType : GLenum {
    Buffer = GL_BUFFER,
    Shader = GL_SHADER,
    Program = GL_PROGRAM,
    VertexArray = GL_VERTEX_ARRAY,
    Query = GL_QUERY,
    ProgramPipeline = GL_PROGRAM_PIPELINE,
    TransformFeedback = GL_TRANSFORM_FEEDBACK,
    Sampler = GL_SAMPLER,
    Texture = GL_TEXTURE,
    Renderbuffer = GL_RENDERBUFFER,
    Framebuffer = GL_FRAMEBUFFER,
};

/// @brief Controls which debug messages are reported (source/type/severity filters).
inline void debugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, bool enabled) {
    if (functions().glDebugMessageControl)
        functions().glDebugMessageControl(source, type, severity, count, ids, enabled ? GL_TRUE : GL_FALSE);
}
/// @brief Disables all debug output (convenience).
inline void debugMessageControlAll(bool enabled) {
    debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, enabled);
}
/// @brief Inserts a user message into the debug log.
inline void debugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, const char* message) {
    if (functions().glDebugMessageInsert && message) {
        const GLsizei len = static_cast<GLsizei>(std::char_traits<char>::length(message));
        functions().glDebugMessageInsert(source, type, id, severity, len, message);
    }
}
/// @brief Polls up to `count` logged debug messages into `log` (returns the number read).
inline GLuint getDebugMessageLog(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids,
                                 GLenum* severities, GLsizei* lengths, GLchar* messageLog) {
    if (!functions().glGetDebugMessageLog) return 0;
    return functions().glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths, messageLog);
}
/// @brief Opens a debug group (shows up as a region in debuggers).
inline void pushDebugGroup(GLenum source, GLuint id, const char* message) {
    if (functions().glPushDebugGroup && message) {
        const GLsizei len = static_cast<GLsizei>(std::char_traits<char>::length(message));
        functions().glPushDebugGroup(source, id, len, message);
    }
}
inline void popDebugGroup() { if (functions().glPopDebugGroup) functions().glPopDebugGroup(); }
/// @brief Labels a GL object (visible in RenderDoc/NSight).
inline void objectLabel(ObjectType type, GLuint name, const char* label) {
    if (functions().glObjectLabel && label) {
        const GLsizei len = static_cast<GLsizei>(std::char_traits<char>::length(label));
        functions().glObjectLabel(static_cast<GLenum>(type), name, len, label);
    }
}
/// @brief Reads back an object label (e.g. for tests). Returns the label string.
inline std::string getObjectLabel(ObjectType type, GLuint name) {
    if (!functions().glGetObjectLabel) return {};
    GLsizei len = 0;
    functions().glGetObjectLabel(static_cast<GLenum>(type), name, 0, &len, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    functions().glGetObjectLabel(static_cast<GLenum>(type), name, len, &len, out.data());
    out.resize(static_cast<size_t>(len));
    return out;
}
inline void objectPtrLabel(const void* ptr, const char* label) {
    if (functions().glObjectPtrLabel && label) {
        const GLsizei len = static_cast<GLsizei>(std::char_traits<char>::length(label));
        functions().glObjectPtrLabel(ptr, len, label);
    }
}

} // namespace fuhgl::gl
