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
    Patches = GL_PATCHES, ///< tessellation input (GL 4.0)
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

// --- F13: enums ---------------------------------------------------------------------
/// @brief Blend equations.
enum class BlendEquation : GLenum {
    FuncAdd = GL_FUNC_ADD, FuncSubtract = GL_FUNC_SUBTRACT, FuncReverseSubtract = GL_FUNC_REVERSE_SUBTRACT,
    Min = GL_MIN, Max = GL_MAX,
};
/// @brief Logic ops (glLogicOp).
enum class LogicOp : GLenum {
    Clear = GL_CLEAR, Set = GL_SET, Copy = GL_COPY, Noop = GL_NOOP, And = GL_AND, Nand = GL_NAND,
    Or = GL_OR, Nor = GL_NOR, Xor = GL_XOR, Equiv = GL_EQUIV, Invert = GL_INVERT,
    AndReverse = GL_AND_REVERSE, AndInverted = GL_AND_INVERTED, OrReverse = GL_OR_REVERSE, OrInverted = GL_OR_INVERTED,
};
/// @brief Stencil ops.
enum class StencilOp : GLenum {
    Keep = GL_KEEP, Zero = GL_ZERO, Replace = GL_REPLACE, Incr = GL_INCR, Decr = GL_DECR,
    Invert = GL_INVERT, IncrWrap = GL_INCR_WRAP, DecrWrap = GL_DECR_WRAP,
};
/// @brief Polygon rasterization modes.
enum class PolyMode : GLenum { Point = GL_POINT, Line = GL_LINE, Fill = GL_FILL };
/// @brief Provoking vertex convention (GL 4.0).
enum class ProvokingVertex : GLenum { First = GL_FIRST_VERTEX_CONVENTION, Last = GL_LAST_VERTEX_CONVENTION };
/// @brief Hint targets/modes.
enum class HintTarget : GLenum {
    LineSmooth = GL_LINE_SMOOTH_HINT, PolygonSmooth = GL_POLYGON_SMOOTH_HINT,
    TextureCompression = GL_TEXTURE_COMPRESSION_HINT, FragmentShaderDerivative = GL_FRAGMENT_SHADER_DERIVATIVE_HINT,
};
enum class HintMode : GLenum { DontCare = GL_DONT_CARE, Fastest = GL_FASTEST, Nicest = GL_NICEST };

// --- F13: blending / logic / stencil --------------------------------------------------
inline void blendEquation(BlendEquation e) { if (functions().glBlendEquation) functions().glBlendEquation(static_cast<GLenum>(e)); }
inline void blendEquationSeparate(BlendEquation rgb, BlendEquation a) {
    if (functions().glBlendEquationSeparate) functions().glBlendEquationSeparate(static_cast<GLenum>(rgb), static_cast<GLenum>(a));
}
inline void blendFuncSeparate(BlendFactor sr, BlendFactor dr, BlendFactor sa, BlendFactor da) {
    if (functions().glBlendFuncSeparate) functions().glBlendFuncSeparate(static_cast<GLenum>(sr), static_cast<GLenum>(dr), static_cast<GLenum>(sa), static_cast<GLenum>(da));
}
inline void blendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { if (functions().glBlendColor) functions().glBlendColor(r, g, b, a); }
inline void logicOp(LogicOp op) { if (functions().glLogicOp) functions().glLogicOp(static_cast<GLenum>(op)); }
inline void enablei(Cap cap, GLuint index) { if (functions().glEnablei) functions().glEnablei(static_cast<GLenum>(cap), index); }
inline void disablei(Cap cap, GLuint index) { if (functions().glDisablei) functions().glDisablei(static_cast<GLenum>(cap), index); }
inline bool isEnabledi(Cap cap, GLuint index) { return functions().glIsEnabledi && functions().glIsEnabledi(static_cast<GLenum>(cap), index) == GL_TRUE; }
inline void colorMaski(GLuint index, bool r, bool g, bool b, bool a) {
    if (functions().glColorMaski) functions().glColorMaski(index, r, g, b, a);
}
inline void stencilFunc(CompareFunc func, GLint ref, GLuint mask) {
    if (functions().glStencilFunc) functions().glStencilFunc(static_cast<GLenum>(func), ref, mask);
}
inline void stencilFuncSeparate(Face face, CompareFunc func, GLint ref, GLuint mask) {
    if (functions().glStencilFuncSeparate) functions().glStencilFuncSeparate(static_cast<GLenum>(face), static_cast<GLenum>(func), ref, mask);
}
inline void stencilOp(StencilOp sfail, StencilOp dpfail, StencilOp dppass) {
    if (functions().glStencilOp) functions().glStencilOp(static_cast<GLenum>(sfail), static_cast<GLenum>(dpfail), static_cast<GLenum>(dppass));
}
inline void stencilOpSeparate(Face face, StencilOp sfail, StencilOp dpfail, StencilOp dppass) {
    if (functions().glStencilOpSeparate) functions().glStencilOpSeparate(static_cast<GLenum>(face), static_cast<GLenum>(sfail), static_cast<GLenum>(dpfail), static_cast<GLenum>(dppass));
}
inline void stencilMask(GLuint mask) { if (functions().glStencilMask) functions().glStencilMask(mask); }
inline void stencilMaskSeparate(Face face, GLuint mask) {
    if (functions().glStencilMaskSeparate) functions().glStencilMaskSeparate(static_cast<GLenum>(face), mask);
}

// --- F13: rasterizer / viewport / scissor ----------------------------------------------
inline void polygonMode(Face face, PolyMode mode) {
    if (functions().glPolygonMode) functions().glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode));
}
inline void polygonOffset(GLfloat factor, GLfloat units) { if (functions().glPolygonOffset) functions().glPolygonOffset(factor, units); }
inline void lineWidth(GLfloat w) { if (functions().glLineWidth) functions().glLineWidth(w); }
inline void pointSize(GLfloat s) { if (functions().glPointSize) functions().glPointSize(s); }
inline void sampleCoverage(GLfloat value, bool invert) {
    if (functions().glSampleCoverage) functions().glSampleCoverage(value, invert ? GL_TRUE : GL_FALSE);
}
inline void sampleMaski(GLuint maskNumber, GLbitfield mask) { if (functions().glSampleMaski) functions().glSampleMaski(maskNumber, mask); }
inline void minSampleShading(GLfloat v) { if (functions().glMinSampleShading) functions().glMinSampleShading(v); }
inline void depthRange(GLdouble n, GLdouble f) { if (functions().glDepthRange) functions().glDepthRange(n, f); }
inline void depthRangef(GLfloat n, GLfloat f) { if (functions().glDepthRangef) functions().glDepthRangef(n, f); }
inline void depthRangeArrayv(GLuint first, GLsizei count, const GLdouble* v) {
    if (functions().glDepthRangeArrayv) functions().glDepthRangeArrayv(first, count, v);
}
inline void depthRangeIndexed(GLuint index, GLdouble n, GLdouble f) {
    if (functions().glDepthRangeIndexed) functions().glDepthRangeIndexed(index, n, f);
}
inline void viewportArrayv(GLuint first, GLsizei count, const GLfloat* v) {
    if (functions().glViewportArrayv) functions().glViewportArrayv(first, count, v);
}
inline void viewportIndexedf(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
    if (functions().glViewportIndexedf) functions().glViewportIndexedf(index, x, y, w, h);
}
inline void scissorArrayv(GLuint first, GLsizei count, const GLint* v) {
    if (functions().glScissorArrayv) functions().glScissorArrayv(first, count, v);
}
inline void scissorIndexed(GLuint index, GLint left, GLint bottom, GLsizei w, GLsizei h) {
    if (functions().glScissorIndexed) functions().glScissorIndexed(index, left, bottom, w, h);
}
inline void provokingVertex(ProvokingVertex mode) { if (functions().glProvokingVertex) functions().glProvokingVertex(static_cast<GLenum>(mode)); }
inline void primitiveRestartIndex(GLuint index) { if (functions().glPrimitiveRestartIndex) functions().glPrimitiveRestartIndex(index); }
inline void hint(HintTarget target, HintMode mode) {
    if (functions().glHint) functions().glHint(static_cast<GLenum>(target), static_cast<GLenum>(mode));
}
inline void patchParameteri(GLenum pname, GLint value) { if (functions().glPatchParameteri) functions().glPatchParameteri(pname, value); }
inline void patchParameterfv(GLenum pname, const GLfloat* values) { if (functions().glPatchParameterfv) functions().glPatchParameterfv(pname, values); }

// --- F13: framebuffer helpers ------------------------------------------------------------
inline void drawBuffer(GLenum buf) { if (functions().glDrawBuffer) functions().glDrawBuffer(buf); }
inline void framebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    if (functions().glFramebufferTexture) functions().glFramebufferTexture(target, attachment, texture, level);
}
inline void framebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    if (functions().glFramebufferTextureLayer) functions().glFramebufferTextureLayer(target, attachment, texture, level, layer);
}
inline void clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value) {
    if (functions().glClearBufferuiv) functions().glClearBufferuiv(buffer, drawbuffer, value);
}
inline void clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    if (functions().glClearBufferfi) functions().glClearBufferfi(buffer, drawbuffer, depth, stencil);
}
inline void invalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments) {
    if (functions().glInvalidateFramebuffer) functions().glInvalidateFramebuffer(target, numAttachments, attachments);
}
inline void invalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei w, GLsizei h) {
    if (functions().glInvalidateSubFramebuffer) functions().glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, w, h);
}
inline GLint getFramebufferAttachmentParameteri(GLenum target, GLenum attachment, GLenum pname) {
    GLint v = 0;
    if (functions().glGetFramebufferAttachmentParameteriv) functions().glGetFramebufferAttachmentParameteriv(target, attachment, pname, &v);
    return v;
}

// --- F13: draw batching --------------------------------------------------------------------
inline void drawRangeElements(Primitive mode, GLuint start, GLuint end, GLsizei count, GLenum indexType, const void* indices) {
    if (functions().glDrawRangeElements) functions().glDrawRangeElements(static_cast<GLenum>(mode), start, end, count, indexType, indices);
}
inline void drawElementsBaseVertex(Primitive mode, GLsizei count, GLenum indexType, const void* indices, GLint baseVertex) {
    if (functions().glDrawElementsBaseVertex) functions().glDrawElementsBaseVertex(static_cast<GLenum>(mode), count, indexType, indices, baseVertex);
}
inline void drawRangeElementsBaseVertex(Primitive mode, GLuint start, GLuint end, GLsizei count, GLenum indexType, const void* indices, GLint baseVertex) {
    if (functions().glDrawRangeElementsBaseVertex) functions().glDrawRangeElementsBaseVertex(static_cast<GLenum>(mode), start, end, count, indexType, indices, baseVertex);
}
inline void drawElementsInstancedBaseVertex(Primitive mode, GLsizei count, GLenum indexType, const void* indices, GLsizei instanceCount, GLint baseVertex) {
    if (functions().glDrawElementsInstancedBaseVertex) functions().glDrawElementsInstancedBaseVertex(static_cast<GLenum>(mode), count, indexType, indices, instanceCount, baseVertex);
}
inline void drawArraysInstancedBaseInstance(Primitive mode, GLint first, GLsizei count, GLsizei instanceCount, GLuint baseInstance) {
    if (functions().glDrawArraysInstancedBaseInstance) functions().glDrawArraysInstancedBaseInstance(static_cast<GLenum>(mode), first, count, instanceCount, baseInstance);
}
inline void drawElementsInstancedBaseInstance(Primitive mode, GLsizei count, GLenum indexType, const void* indices, GLsizei instanceCount, GLuint baseInstance) {
    if (functions().glDrawElementsInstancedBaseInstance) functions().glDrawElementsInstancedBaseInstance(static_cast<GLenum>(mode), count, indexType, indices, instanceCount, baseInstance);
}
inline void drawElementsInstancedBaseVertexBaseInstance(Primitive mode, GLsizei count, GLenum indexType, const void* indices, GLsizei instanceCount, GLint baseVertex, GLuint baseInstance) {
    if (functions().glDrawElementsInstancedBaseVertexBaseInstance) functions().glDrawElementsInstancedBaseVertexBaseInstance(static_cast<GLenum>(mode), count, indexType, indices, instanceCount, baseVertex, baseInstance);
}
inline void multiDrawArrays(Primitive mode, const GLint* first, const GLsizei* count, GLsizei drawCount) {
    if (functions().glMultiDrawArrays) functions().glMultiDrawArrays(static_cast<GLenum>(mode), first, count, drawCount);
}
inline void multiDrawElements(Primitive mode, const GLsizei* count, GLenum indexType, const void* const* indices, GLsizei drawCount) {
    if (functions().glMultiDrawElements) functions().glMultiDrawElements(static_cast<GLenum>(mode), count, indexType, indices, drawCount);
}
inline void multiDrawElementsBaseVertex(Primitive mode, const GLsizei* count, GLenum indexType, const void* const* indices, GLsizei drawCount, const GLint* baseVertex) {
    if (functions().glMultiDrawElementsBaseVertex) functions().glMultiDrawElementsBaseVertex(static_cast<GLenum>(mode), count, indexType, indices, drawCount, baseVertex);
}
inline void multiDrawArraysIndirect(Primitive mode, const void* indirect, GLsizei drawCount, GLsizei stride) {
    if (functions().glMultiDrawArraysIndirect) functions().glMultiDrawArraysIndirect(static_cast<GLenum>(mode), indirect, drawCount, stride);
}
inline void multiDrawElementsIndirect(Primitive mode, GLenum indexType, const void* indirect, GLsizei drawCount, GLsizei stride) {
    if (functions().glMultiDrawElementsIndirect) functions().glMultiDrawElementsIndirect(static_cast<GLenum>(mode), indexType, indirect, drawCount, stride);
}

// --- F13: queries / sync / generic getters ---------------------------------------------------
inline GLint getQueryi(GLenum target, GLenum pname) {
    GLint v = 0;
    if (functions().glGetQueryiv) functions().glGetQueryiv(target, pname, &v);
    return v;
}
inline void beginQueryIndexed(GLenum target, GLuint index, GLuint id) {
    if (functions().glBeginQueryIndexed) functions().glBeginQueryIndexed(target, index, id);
}
inline void endQueryIndexed(GLenum target, GLuint index) { if (functions().glEndQueryIndexed) functions().glEndQueryIndexed(target, index); }
inline void waitSync(GLsync sync, GLuint flags, GLuint64 timeout) {
    if (functions().glWaitSync) functions().glWaitSync(sync, flags, timeout);
}
inline GLint getSynci(GLsync sync, GLenum pname) {
    GLint v = 0;
    if (functions().glGetSynciv) { GLsizei len = 0; functions().glGetSynciv(sync, pname, 1, &len, &v); }
    return v;
}
inline bool getBoolean(GLenum pname) {
    GLboolean v = GL_FALSE;
    if (functions().glGetBooleanv) functions().glGetBooleanv(pname, &v);
    return v == GL_TRUE;
}
inline GLint getIntegeri(GLenum target, GLuint index) {
    GLint v = 0;
    if (functions().glGetIntegeri_v) functions().glGetIntegeri_v(target, index, &v);
    return v;
}
inline GLint64 getInteger64i(GLenum target, GLuint index) {
    GLint64 v = 0;
    if (functions().glGetInteger64i_v) functions().glGetInteger64i_v(target, index, &v);
    return v;
}
inline bool isEnabled(Cap cap) { return functions().glIsEnabled && functions().glIsEnabled(static_cast<GLenum>(cap)) == GL_TRUE; }
inline bool isBuffer(GLuint id) { return functions().glIsBuffer && functions().glIsBuffer(id) == GL_TRUE; }
inline bool isTexture(GLuint id) { return functions().glIsTexture && functions().glIsTexture(id) == GL_TRUE; }
inline bool isVertexArray(GLuint id) { return functions().glIsVertexArray && functions().glIsVertexArray(id) == GL_TRUE; }
inline bool isFramebuffer(GLuint id) { return functions().glIsFramebuffer && functions().glIsFramebuffer(id) == GL_TRUE; }
inline bool isRenderbuffer(GLuint id) { return functions().glIsRenderbuffer && functions().glIsRenderbuffer(id) == GL_TRUE; }
inline bool isProgram(GLuint id) { return functions().glIsProgram && functions().glIsProgram(id) == GL_TRUE; }
inline bool isShader(GLuint id) { return functions().glIsShader && functions().glIsShader(id) == GL_TRUE; }
inline bool isSync(GLsync sync) { return functions().glIsSync && functions().glIsSync(sync) == GL_TRUE; }
inline bool isQuery(GLuint id) { return functions().glIsQuery && functions().glIsQuery(id) == GL_TRUE; }
inline bool isSampler(GLuint id) { return functions().glIsSampler && functions().glIsSampler(id) == GL_TRUE; }

// --- F13: internal formats / image ops ---------------------------------------------------------
inline GLint getInternalformati(GLenum target, GLenum internalformat, GLenum pname) {
    GLint v = 0;
    if (functions().glGetInternalformativ) functions().glGetInternalformativ(target, internalformat, pname, 1, &v);
    return v;
}
inline GLint64 getInternalformati64(GLenum target, GLenum internalformat, GLenum pname) {
    GLint64 v = 0;
    if (functions().glGetInternalformati64v) functions().glGetInternalformati64v(target, internalformat, pname, 1, &v);
    return v;
}
inline void copyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ,
                             GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
                             GLsizei w, GLsizei h, GLsizei d) {
    if (functions().glCopyImageSubData)
        functions().glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ,
                                       dstName, dstTarget, dstLevel, dstX, dstY, dstZ, w, h, d);
}
inline void clearTexImage(GLuint texture, GLint level, GLenum format, GLenum type, const void* data) {
    if (functions().glClearTexImage) functions().glClearTexImage(texture, level, format, type, data);
}
inline void clearTexSubImage(GLuint texture, GLint level, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d,
                             GLenum format, GLenum type, const void* data) {
    if (functions().glClearTexSubImage) functions().glClearTexSubImage(texture, level, x, y, z, w, h, d, format, type, data);
}

} // namespace fuhgl::gl
