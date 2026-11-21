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
/// @brief Indirect compute dispatch: reads num_groups_x/y/z from the GL_DISPATCH_INDIRECT_BUFFER.
inline void dispatchComputeIndirect(GLintptr indirect) {
    if (functions().glDispatchComputeIndirect) functions().glDispatchComputeIndirect(indirect);
}
inline void memoryBarrier(GLbitfield barriers) { functions().glMemoryBarrier(barriers); }

/// @brief GPU-side copy between two bound buffer targets (GL_COPY_READ/WRITE_BUFFER).
inline void copyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    if (functions().glCopyBufferSubData) functions().glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

/// @brief Reads back a texture level into host memory (bind the texture / use a PIXEL_PACK_BUFFER first).
inline void getTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) {
    if (functions().glGetTexImage) functions().glGetTexImage(target, level, format, type, pixels);
}
/// @brief Reads back a compressed texture level (buffer must hold GL_TEXTURE_COMPRESSED_IMAGE_SIZE bytes).
inline void getCompressedTexImage(GLenum target, GLint level, void* img) {
    if (functions().glGetCompressedTexImage) functions().glGetCompressedTexImage(target, level, img);
}

/// @brief Binds a texture level as a compute image (GL 4.2): access = GL_READ_ONLY/WRITE_ONLY/READ_WRITE,
/// format = sized internal format (e.g. GL_RGBA8). layered=false + layer for array/cube slices.
inline void bindImageTexture(GLuint unit, GLuint texture, GLint level, bool layered, GLint layer, GLenum access, GLenum format) {
    if (functions().glBindImageTexture)
        functions().glBindImageTexture(unit, texture, level, layered ? GL_TRUE : GL_FALSE, layer, access, format);
}
/// @brief Binds a range of consecutive image units to textures (GL 4.2+).
inline void bindImageTextures(GLuint first, GLsizei count, const GLuint* textures) {
    if (functions().glBindImageTextures) functions().glBindImageTextures(first, count, textures);
}

// --- indirect draws (GL 4.0) ------------------------------------------------------
inline void drawArraysIndirect(Primitive mode, const void* indirect) {
    functions().glDrawArraysIndirect(static_cast<GLenum>(mode), indirect);
}
inline void drawElementsIndirect(Primitive mode, GLenum indexType, const void* indirect) {
    functions().glDrawElementsIndirect(static_cast<GLenum>(mode), indexType, indirect);
}

// --- samplers (GL 3.3) -------------------------------------------------------------
inline void bindSampler(GLuint unit, GLuint sampler) { functions().glBindSampler(unit, sampler); }

/// @brief Selects the draw buffers of the bound DRAW_FRAMEBUFFER (MRT, e.g. deferred).
inline void drawBuffers(GLsizei n, const GLenum* bufs) { functions().glDrawBuffers(n, bufs); }

} // namespace fuhgl::gl
