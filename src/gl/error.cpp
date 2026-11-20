#include "fuhgl/gl/error.hpp"
#include "fuhgl/gl/loader.hpp"

namespace fuhgl::gl {
namespace {
DebugCallback g_debugCallback;

void debugTrampoline(GLenum source, GLenum type, GLuint id, GLenum severity,
                     GLsizei length, const GLchar* message, const void* /*userParam*/) {
    if (!g_debugCallback) return;
    DebugMessage msg{source, type, id, severity, message ? std::string(message, static_cast<std::size_t>(length)) : std::string{}};
    g_debugCallback(msg);
}
} // namespace

std::string errorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        default: return "GL_UNKNOWN_ERROR(" + std::to_string(error) + ")";
    }
}

Error lastError() {
    if (isInitialized() && functions().glGetError)
        return static_cast<Error>(functions().glGetError());
    return Error::NoError;
}

void clearErrors() {
    if (isInitialized() && functions().glGetError)
        while (functions().glGetError() != GL_NO_ERROR) {}
}

void setDebugCallback(DebugCallback callback, bool synchronous) {
    g_debugCallback = std::move(callback);
    if (!isInitialized() || !functions().glDebugMessageCallback) return;
    functions().glDebugMessageCallback(g_debugCallback ? debugTrampoline : nullptr, nullptr);
    if (functions().glEnable) {
        functions().glEnable(GL_DEBUG_OUTPUT);
        if (synchronous) functions().glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }
}

} // namespace fuhgl::gl
