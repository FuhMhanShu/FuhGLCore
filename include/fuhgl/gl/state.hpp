#pragma once

/// @file state.hpp
/// @brief Thin free-function wrappers around common GL state and draw calls.
///
/// These are deliberately stateless pass-throughs (no state tracking, no
/// overhead beyond the GL call itself) and require gl::init() to have run.

#include "loader.hpp"
#include "types.hpp"

namespace fuhgl::gl {

/// @brief Clear buffer bits (bitwise OR-able).
enum class ClearBit : GLbitfield {
    Depth = GL_DEPTH_BUFFER_BIT,
    Stencil = GL_STENCIL_BUFFER_BIT,
    Color = GL_COLOR_BUFFER_BIT,
};
constexpr ClearBit operator|(ClearBit a, ClearBit b) {
    return static_cast<ClearBit>(static_cast<GLbitfield>(a) | static_cast<GLbitfield>(b));
}

/// @brief Primitive topology for draw calls.
enum class Primitive : GLenum {
    Points = GL_POINTS,
    Lines = GL_LINES,
    LineStrip = GL_LINE_STRIP,
    Triangles = GL_TRIANGLES,
    TriangleStrip = GL_TRIANGLE_STRIP,
};

/// @brief Enable-able capabilities.
enum class Cap : GLenum {
    DepthTest = GL_DEPTH_TEST,
    Blend = GL_BLEND,
    CullFace = GL_CULL_FACE,
    ScissorTest = GL_SCISSOR_TEST,
    StencilTest = GL_STENCIL_TEST,
    DebugOutput = GL_DEBUG_OUTPUT,
    DebugOutputSynchronous = GL_DEBUG_OUTPUT_SYNCHRONOUS,
    FramebufferSrgb = GL_FRAMEBUFFER_SRGB,
};

/// @brief Blend factors.
enum class BlendFactor : GLenum {
    Zero = GL_ZERO,
    One = GL_ONE,
    SrcAlpha = GL_SRC_ALPHA,
    OneMinusSrcAlpha = GL_ONE_MINUS_SRC_ALPHA,
    DstAlpha = GL_DST_ALPHA,
    OneMinusDstAlpha = GL_ONE_MINUS_DST_ALPHA,
};

/// @brief Depth/stencil compare functions.
enum class CompareFunc : GLenum {
    Never = GL_NEVER,
    Less = GL_LESS,
    Equal = GL_EQUAL,
    LessEqual = GL_LEQUAL,
    Greater = GL_GREATER,
    NotEqual = GL_NOTEQUAL,
    GreaterEqual = GL_GEQUAL,
    Always = GL_ALWAYS,
};

/// @brief Face culling mode.
enum class Face : GLenum { Front = GL_FRONT, Back = GL_BACK, FrontAndBack = GL_FRONT_AND_BACK };

inline void viewport(GLint x, GLint y, GLsizei w, GLsizei h) { functions().glViewport(x, y, w, h); }
inline void scissor(GLint x, GLint y, GLsizei w, GLsizei h) { functions().glScissor(x, y, w, h); }
inline void clearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { functions().glClearColor(r, g, b, a); }
inline void clear(ClearBit mask) { functions().glClear(static_cast<GLbitfield>(mask)); }
inline void enable(Cap cap) { functions().glEnable(static_cast<GLenum>(cap)); }
inline void disable(Cap cap) { functions().glDisable(static_cast<GLenum>(cap)); }
inline void blendFunc(BlendFactor src, BlendFactor dst) {
    functions().glBlendFunc(static_cast<GLenum>(src), static_cast<GLenum>(dst));
}
inline void depthFunc(CompareFunc f) { functions().glDepthFunc(static_cast<GLenum>(f)); }
inline void depthMask(bool flag) { functions().glDepthMask(flag ? GL_TRUE : GL_FALSE); }
inline void colorMask(bool r, bool g, bool b, bool a) {
    functions().glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE, b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE);
}
inline void cullFace(Face f) { functions().glCullFace(static_cast<GLenum>(f)); }
/// @brief frontFace(true) selects counter-clockwise as front.
inline void frontFace(bool ccw) { functions().glFrontFace(ccw ? GL_CCW : GL_CW); }
inline void pixelStorei(GLenum pname, GLint param) { functions().glPixelStorei(pname, param); }
inline void activeTexture(GLenum unit) { functions().glActiveTexture(unit); }

inline void drawArrays(Primitive mode, GLint first, GLsizei count) {
    functions().glDrawArrays(static_cast<GLenum>(mode), first, count);
}
inline void drawElements(Primitive mode, GLsizei count, GLenum indexType, const void* indices) {
    functions().glDrawElements(static_cast<GLenum>(mode), count, indexType, indices);
}
inline void drawArraysInstanced(Primitive mode, GLint first, GLsizei count, GLsizei instances) {
    functions().glDrawArraysInstanced(static_cast<GLenum>(mode), first, count, instances);
}
inline void drawElementsInstanced(Primitive mode, GLsizei count, GLenum indexType, const void* indices, GLsizei instances) {
    functions().glDrawElementsInstanced(static_cast<GLenum>(mode), count, indexType, indices, instances);
}

inline void finish() { functions().glFinish(); }
inline void flush() { functions().glFlush(); }

// --- pixels / clears -----------------------------------------------------------
inline void readPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* data) {
    functions().glReadPixels(x, y, w, h, format, type, data);
}
inline void readBuffer(GLenum src) { functions().glReadBuffer(src); }
inline void clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) {
    functions().glClearBufferfv(buffer, drawbuffer, value);
}
inline void clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) {
    functions().glClearBufferiv(buffer, drawbuffer, value);
}

// --- compute (GL 4.3) ------------------------------------------------------------
inline void dispatchCompute(GLuint x, GLuint y, GLuint z) { functions().glDispatchCompute(x, y, z); }
inline void memoryBarrier(GLbitfield barriers) { functions().glMemoryBarrier(barriers); }

// --- indirect draws (GL 4.0) ------------------------------------------------------
inline void drawArraysIndirect(Primitive mode, const void* indirect) {
    functions().glDrawArraysIndirect(static_cast<GLenum>(mode), indirect);
}
inline void drawElementsIndirect(Primitive mode, GLenum indexType, const void* indirect) {
    functions().glDrawElementsIndirect(static_cast<GLenum>(mode), indexType, indirect);
}

// --- samplers (GL 3.3) -------------------------------------------------------------
inline void bindSampler(GLuint unit, GLuint sampler) { functions().glBindSampler(unit, sampler); }

} // namespace fuhgl::gl
