#pragma once

/// @file error.hpp
/// @brief glGetError wrapper, GL debug-output callback and the debug assert macro.
///
/// No exceptions are used anywhere in FuhGLCore: runtime GL errors are queried
/// via lastError(), and in debug builds FUHGL_ASSERT aborts on failure.

#include "types.hpp"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

/// @brief Debug assertion: prints file/line and aborts in debug builds, no-op in release.
#ifndef NDEBUG
    #define FUHGL_ASSERT(cond)                                                              \
        do {                                                                                \
            if (!(cond)) {                                                                  \
                std::fprintf(stderr, "FUHGL_ASSERT failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
                std::abort();                                                               \
            }                                                                               \
        } while (0)
#else
    #define FUHGL_ASSERT(cond) do { (void)sizeof(cond); } while (0)
#endif

namespace fuhgl::gl {

/// @brief Typed glGetError codes.
enum class Error : GLenum {
    NoError = GL_NO_ERROR,
    InvalidEnum = GL_INVALID_ENUM,
    InvalidValue = GL_INVALID_VALUE,
    InvalidOperation = GL_INVALID_OPERATION,
    InvalidFramebufferOperation = GL_INVALID_FRAMEBUFFER_OPERATION,
    OutOfMemory = GL_OUT_OF_MEMORY,
};

/// @brief Human-readable name of a GL error code.
std::string errorString(GLenum error);

/// @brief Returns and clears the oldest pending GL error (NoError if none).
Error lastError();

/// @brief Drains all pending GL errors, discarding them.
void clearErrors();

/// @brief One debug message delivered by the GL debug output (GL 4.3 / KHR_debug).
struct DebugMessage {
    GLenum source;
    GLenum type;
    GLuint id;
    GLenum severity;
    std::string message;
};

/// @brief User callback for GL debug messages.
using DebugCallback = std::function<void(const DebugMessage&)>;

/// @brief Install a debug message callback (requires GL 4.3 or KHR_debug; no-op otherwise).
/// @param synchronous also enable GL_DEBUG_OUTPUT_SYNCHRONOUS so errors surface at the call site.
void setDebugCallback(DebugCallback callback, bool synchronous = true);

} // namespace fuhgl::gl
