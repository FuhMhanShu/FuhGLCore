#pragma once

/// @file loader.hpp
/// @brief Runtime OpenGL function loader — zero dependencies.
///
/// Call fuhgl::gl::init() once after an OpenGL context is current
/// (fuhgl::Window does this automatically). The curated table below covers the
/// core functions used by the wrappers plus common entry points up to GL 4.6
/// (where the platform provides them — macOS caps at GL 4.1);
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
    void (*glGetInteger64v)(GLenum pname, GLint64* data);
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
    GLuint (*glGetUniformBlockIndex)(GLuint program, const GLchar* uniformBlockName);
    void (*glGetUniformiv)(GLuint program, GLint location, GLint* params);
    void (*glGetUniformfv)(GLuint program, GLint location, GLfloat* params);
    void (*glGetUniformuiv)(GLuint program, GLint location, GLuint* params);
    void (*glUniform1ui)(GLint location, GLuint v0);
    void (*glUniform2ui)(GLint location, GLuint v0, GLuint v1);
    void (*glUniform3ui)(GLint location, GLuint v0, GLuint v1, GLuint v2);
    void (*glUniform4ui)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
    void (*glUniform1uiv)(GLint location, GLsizei count, const GLuint* value);
    void (*glUniform2uiv)(GLint location, GLsizei count, const GLuint* value);
    void (*glUniform3uiv)(GLint location, GLsizei count, const GLuint* value);
    void (*glUniform4uiv)(GLint location, GLsizei count, const GLuint* value);
    GLuint (*glGetShaderStorageBlockIndex)(GLuint program, const GLchar* storageBlockName);
    void (*glShaderStorageBlockBinding)(GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding);
    // buffers
    void (*glGenBuffers)(GLsizei n, GLuint* buffers);
    void (*glBindBuffer)(GLenum target, GLuint buffer);
    void (*glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
    void (*glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
    void (*glDeleteBuffers)(GLsizei n, const GLuint* buffers);
    void* (*glMapBuffer)(GLenum target, GLenum access);
    void* (*glMapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
    GLboolean (*glUnmapBuffer)(GLenum target);
    void (*glFlushMappedBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length);
    void (*glCopyBufferSubData)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
    void (*glGetBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, void* data);
    void (*glGetBufferParameteriv)(GLenum target, GLenum pname, GLint* params);
    void (*glGetBufferParameteri64v)(GLenum target, GLenum pname, GLint64* params);
    void (*glInvalidateBufferData)(GLuint buffer);
    void (*glInvalidateBufferSubData)(GLuint buffer, GLintptr offset, GLsizeiptr length);
    void (*glBindBufferBase)(GLenum target, GLuint index, GLuint buffer);
    void (*glBindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
    // vertex arrays
    void (*glGenVertexArrays)(GLsizei n, GLuint* arrays);
    void (*glBindVertexArray)(GLuint array);
    void (*glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
    void (*glEnableVertexAttribArray)(GLuint index);
    void (*glDisableVertexAttribArray)(GLuint index);
    void (*glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
    void (*glVertexAttribIPointer)(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
    void (*glVertexAttribLPointer)(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
    void (*glVertexAttribDivisor)(GLuint index, GLuint divisor);
    // vertex attrib binding (GL 4.3)
    void (*glBindVertexBuffer)(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
    void (*glBindVertexBuffers)(GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides);
    void (*glVertexAttribBinding)(GLuint attribindex, GLuint bindingindex);
    void (*glVertexAttribFormat)(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
    void (*glVertexAttribIFormat)(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
    void (*glVertexAttribLFormat)(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
    void (*glVertexBindingDivisor)(GLuint bindingindex, GLuint divisor);
    // textures
    void (*glGenTextures)(GLsizei n, GLuint* textures);
    void (*glBindTexture)(GLenum target, GLuint texture);
    void (*glDeleteTextures)(GLsizei n, const GLuint* textures);
    void (*glActiveTexture)(GLenum texture);
    void (*glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
    void (*glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
    void (*glTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
    void (*glCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    void (*glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glCopyTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glGetTexImage)(GLenum target, GLint level, GLenum format, GLenum type, void* pixels);
    void (*glGetCompressedTexImage)(GLenum target, GLint level, void* img);
    void (*glGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname, GLint* params);
    void (*glGetTexParameteriv)(GLenum target, GLenum pname, GLint* params);
    void (*glGetTexParameterfv)(GLenum target, GLenum pname, GLfloat* params);
    void (*glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data);
    void (*glCompressedTexImage3D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data);
    void (*glCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
    void (*glCompressedTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
    void (*glTexBuffer)(GLenum target, GLenum internalformat, GLuint buffer);
    void (*glTexBufferRange)(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
    void (*glTextureView)(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers);
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
    // pixels / clears
    void (*glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* data);
    void (*glReadBuffer)(GLenum src);
    void (*glClearBufferfv)(GLenum buffer, GLint drawbuffer, const GLfloat* value);
    void (*glClearBufferiv)(GLenum buffer, GLint drawbuffer, const GLint* value);
    // compute / images (GL 4.2/4.3)
    void (*glDispatchCompute)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
    void (*glDispatchComputeIndirect)(GLintptr indirect);
    void (*glMemoryBarrier)(GLbitfield barriers);
    void (*glBindImageTexture)(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
    void (*glBindImageTextures)(GLuint first, GLsizei count, const GLuint* textures);
    // queries (GL 3.3)
    void (*glGenQueries)(GLsizei n, GLuint* ids);
    void (*glDeleteQueries)(GLsizei n, const GLuint* ids);
    void (*glBeginQuery)(GLenum target, GLuint id);
    void (*glEndQuery)(GLenum target);
    void (*glGetQueryObjectui64v)(GLuint id, GLenum pname, GLuint64* params);
    void (*glQueryCounter)(GLuint id, GLenum target);
    // sync objects (GL 3.2)
    GLsync (*glFenceSync)(GLenum condition, GLuint flags);
    void (*glDeleteSync)(GLsync sync);
    GLenum (*glClientWaitSync)(GLsync sync, GLbitfield flags, GLuint64 timeout);
    // indirect draws (GL 4.0)
    void (*glDrawArraysIndirect)(GLenum mode, const void* indirect);
    void (*glDrawElementsIndirect)(GLenum mode, GLenum type, const void* indirect);
    // samplers (GL 3.3)
    void (*glGenSamplers)(GLsizei n, GLuint* samplers);
    void (*glDeleteSamplers)(GLsizei n, const GLuint* samplers);
    void (*glBindSampler)(GLuint unit, GLuint sampler);
    void (*glSamplerParameteri)(GLuint sampler, GLenum pname, GLint param);
    void (*glSamplerParameterf)(GLuint sampler, GLenum pname, GLfloat param);
    // immutable / multisample storage (GL 4.2 / 3.3)
    void (*glTexStorage2D)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
    void (*glTexStorage3D)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
    void (*glTexStorage2DMultisample)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
    void (*glRenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
    // framebuffers (F13)
    void (*glDrawBuffer)(GLenum buf);
    void (*glFramebufferTexture)(GLenum target, GLenum attachment, GLuint texture, GLint level);
    void (*glFramebufferTextureLayer)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
    void (*glClearBufferuiv)(GLenum buffer, GLint drawbuffer, const GLuint* value);
    void (*glClearBufferfi)(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
    void (*glInvalidateFramebuffer)(GLenum target, GLsizei numAttachments, const GLenum* attachments);
    void (*glInvalidateSubFramebuffer)(GLenum target, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint* params);
    // blending / logic op / stencil (F13)
    void (*glBlendEquation)(GLenum mode);
    void (*glBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
    void (*glBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
    void (*glBlendColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void (*glLogicOp)(GLenum opcode);
    void (*glEnablei)(GLenum target, GLuint index);
    void (*glDisablei)(GLenum target, GLuint index);
    GLboolean (*glIsEnabledi)(GLenum target, GLuint index);
    void (*glBlendEquationi)(GLuint buf, GLenum mode);
    void (*glBlendEquationSeparatei)(GLuint buf, GLenum modeRGB, GLenum modeAlpha);
    void (*glBlendFunci)(GLuint buf, GLenum src, GLenum dst);
    void (*glBlendFuncSeparatei)(GLuint buf, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
    void (*glColorMaski)(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
    void (*glStencilFunc)(GLenum func, GLint ref, GLuint mask);
    void (*glStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
    void (*glStencilOp)(GLenum sfail, GLenum dpfail, GLenum dppass);
    void (*glStencilOpSeparate)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
    void (*glStencilMask)(GLuint mask);
    void (*glStencilMaskSeparate)(GLenum face, GLuint mask);
    // rasterizer / viewport / scissor (F13)
    void (*glPolygonMode)(GLenum face, GLenum mode);
    void (*glPolygonOffset)(GLfloat factor, GLfloat units);
    void (*glLineWidth)(GLfloat width);
    void (*glPointSize)(GLfloat size);
    void (*glPointParameterf)(GLenum pname, GLfloat param);
    void (*glPointParameterfv)(GLenum pname, const GLfloat* params);
    void (*glSampleCoverage)(GLfloat value, GLboolean invert);
    void (*glSampleMaski)(GLuint maskNumber, GLbitfield mask);
    void (*glMinSampleShading)(GLfloat value);
    void (*glDepthRange)(GLdouble n, GLdouble f);
    void (*glDepthRangef)(GLfloat n, GLfloat f);
    void (*glDepthRangeArrayv)(GLuint first, GLsizei count, const GLdouble* v);
    void (*glDepthRangeIndexed)(GLuint index, GLdouble n, GLdouble f);
    void (*glViewportArrayv)(GLuint first, GLsizei count, const GLfloat* v);
    void (*glViewportIndexedf)(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h);
    void (*glScissorArrayv)(GLuint first, GLsizei count, const GLint* v);
    void (*glScissorIndexed)(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height);
    void (*glProvokingVertex)(GLenum mode);
    void (*glPrimitiveRestartIndex)(GLuint index);
    void (*glHint)(GLenum target, GLenum mode);
    // patches (tessellation, F13)
    void (*glPatchParameteri)(GLenum pname, GLint value);
    void (*glPatchParameterfv)(GLenum pname, const GLfloat* values);
    // draw batching (F13)
    void (*glDrawRangeElements)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices);
    void (*glDrawElementsBaseVertex)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint basevertex);
    void (*glDrawRangeElementsBaseVertex)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void* indices, GLint basevertex);
    void (*glDrawElementsInstancedBaseVertex)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex);
    void (*glDrawArraysInstancedBaseInstance)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance);
    void (*glDrawElementsInstancedBaseInstance)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLuint baseinstance);
    void (*glDrawElementsInstancedBaseVertexBaseInstance)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance);
    void (*glMultiDrawArrays)(GLenum mode, const GLint* first, const GLsizei* count, GLsizei drawcount);
    void (*glMultiDrawElements)(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount);
    void (*glMultiDrawElementsBaseVertex)(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices, GLsizei drawcount, const GLint* basevertex);
    void (*glMultiDrawArraysIndirect)(GLenum mode, const void* indirect, GLsizei drawcount, GLsizei stride);
    void (*glMultiDrawElementsIndirect)(GLenum mode, GLenum type, const void* indirect, GLsizei drawcount, GLsizei stride);
    // queries / sync / generic getters (F13)
    void (*glGetQueryiv)(GLenum target, GLenum pname, GLint* params);
    void (*glBeginQueryIndexed)(GLenum target, GLuint index, GLuint id);
    void (*glEndQueryIndexed)(GLenum target, GLuint index);
    void (*glWaitSync)(GLsync sync, GLuint flags, GLuint64 timeout);
    void (*glGetSynciv)(GLsync sync, GLenum pname, GLsizei count, GLsizei* length, GLint* values);
    void (*glGetBooleanv)(GLenum pname, GLboolean* data);
    void (*glGetBooleani_v)(GLenum target, GLuint index, GLboolean* data);
    void (*glGetIntegeri_v)(GLenum target, GLuint index, GLint* data);
    void (*glGetInteger64i_v)(GLenum target, GLuint index, GLint64* data);
    GLboolean (*glIsEnabled)(GLenum cap);
    GLboolean (*glIsBuffer)(GLuint buffer);
    GLboolean (*glIsTexture)(GLuint texture);
    GLboolean (*glIsVertexArray)(GLuint array);
    GLboolean (*glIsFramebuffer)(GLuint framebuffer);
    GLboolean (*glIsRenderbuffer)(GLuint renderbuffer);
    GLboolean (*glIsProgram)(GLuint program);
    GLboolean (*glIsShader)(GLuint shader);
    GLboolean (*glIsSync)(GLsync sync);
    GLboolean (*glIsQuery)(GLuint id);
    GLboolean (*glIsSampler)(GLuint sampler);
    // debug (F13)
    void (*glDebugMessageControl)(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
    void (*glDebugMessageInsert)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
    GLuint (*glGetDebugMessageLog)(GLuint count, GLsizei bufSize, GLenum* sources, GLenum* types, GLuint* ids, GLenum* severities, GLsizei* lengths, GLchar* messageLog);
    void (*glPushDebugGroup)(GLenum source, GLuint id, GLsizei length, const GLchar* message);
    void (*glPopDebugGroup)();
    void (*glObjectLabel)(GLenum identifier, GLuint name, GLsizei length, const GLchar* label);
    void (*glGetObjectLabel)(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei* length, GLchar* label);
    void (*glObjectPtrLabel)(const void* ptr, GLsizei length, const GLchar* label);
    void (*glGetObjectPtrLabel)(const void* ptr, GLsizei bufSize, GLsizei* length, GLchar* label);
    // internal formats / image ops (F13)
    void (*glGetInternalformativ)(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint* params);
    void (*glGetInternalformati64v)(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint64* params);
    void (*glCopyImageSubData)(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ,
                               GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
                               GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
    void (*glClearTexImage)(GLuint texture, GLint level, GLenum format, GLenum type, const void* data);
    void (*glClearTexSubImage)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                              GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* data);
    // P3: program pipelines / separable programs (GL 4.1)
    void (*glCreateProgramPipelines)(GLsizei n, GLuint* pipelines);
    void (*glDeleteProgramPipelines)(GLsizei n, const GLuint* pipelines);
    GLboolean (*glIsProgramPipeline)(GLuint pipeline);
    void (*glBindProgramPipeline)(GLuint pipeline);
    void (*glUseProgramStages)(GLuint pipeline, GLbitfield stages, GLuint program);
    void (*glActiveShaderProgram)(GLuint pipeline, GLuint program);
    void (*glValidateProgramPipeline)(GLuint pipeline);
    void (*glGetProgramPipelineiv)(GLuint pipeline, GLenum pname, GLint* params);
    void (*glGetProgramPipelineInfoLog)(GLuint pipeline, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
    GLuint (*glCreateShaderProgramv)(GLenum type, GLsizei count, const GLchar* const* strings);
    void (*glProgramParameteri)(GLuint program, GLenum pname, GLint value);
    void (*glProgramUniform1i)(GLuint program, GLint location, GLint v0);
    void (*glProgramUniform2i)(GLuint program, GLint location, GLint v0, GLint v1);
    void (*glProgramUniform3i)(GLuint program, GLint location, GLint v0, GLint v1, GLint v2);
    void (*glProgramUniform4i)(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
    void (*glProgramUniform1f)(GLuint program, GLint location, GLfloat v0);
    void (*glProgramUniform2f)(GLuint program, GLint location, GLfloat v0, GLfloat v1);
    void (*glProgramUniform3f)(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void (*glProgramUniform4f)(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void (*glProgramUniform1ui)(GLuint program, GLint location, GLuint v0);
    void (*glProgramUniform2ui)(GLuint program, GLint location, GLuint v0, GLuint v1);
    void (*glProgramUniform3ui)(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2);
    void (*glProgramUniform4ui)(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
    void (*glProgramUniform1iv)(GLuint program, GLint location, GLsizei count, const GLint* value);
    void (*glProgramUniform2iv)(GLuint program, GLint location, GLsizei count, const GLint* value);
    void (*glProgramUniform3iv)(GLuint program, GLint location, GLsizei count, const GLint* value);
    void (*glProgramUniform4iv)(GLuint program, GLint location, GLsizei count, const GLint* value);
    void (*glProgramUniform1fv)(GLuint program, GLint location, GLsizei count, const GLfloat* value);
    void (*glProgramUniform2fv)(GLuint program, GLint location, GLsizei count, const GLfloat* value);
    void (*glProgramUniform3fv)(GLuint program, GLint location, GLsizei count, const GLfloat* value);
    void (*glProgramUniform4fv)(GLuint program, GLint location, GLsizei count, const GLfloat* value);
    void (*glProgramUniform1uiv)(GLuint program, GLint location, GLsizei count, const GLuint* value);
    void (*glProgramUniform2uiv)(GLuint program, GLint location, GLsizei count, const GLuint* value);
    void (*glProgramUniform3uiv)(GLuint program, GLint location, GLsizei count, const GLuint* value);
    void (*glProgramUniform4uiv)(GLuint program, GLint location, GLsizei count, const GLuint* value);
    void (*glProgramUniformMatrix2fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix3fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix4fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix2x3fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix2x4fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix3x2fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix3x4fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix4x2fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    void (*glProgramUniformMatrix4x3fv)(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
    // P3: transform feedback (GL 4.0)
    void (*glGenTransformFeedbacks)(GLsizei n, GLuint* ids);
    void (*glDeleteTransformFeedbacks)(GLsizei n, const GLuint* ids);
    GLboolean (*glIsTransformFeedback)(GLuint id);
    void (*glBindTransformFeedback)(GLenum target, GLuint id);
    void (*glBeginTransformFeedback)(GLenum primitiveMode);
    void (*glEndTransformFeedback)();
    void (*glPauseTransformFeedback)();
    void (*glResumeTransformFeedback)();
    void (*glDrawTransformFeedback)(GLenum mode, GLuint id);
    void (*glDrawTransformFeedbackStream)(GLenum mode, GLuint id, GLuint stream);
    void (*glTransformFeedbackVaryings)(GLuint program, GLsizei count, const GLchar* const* varyings, GLenum bufferMode);
    void (*glGetTransformFeedbackVarying)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLsizei* size, GLenum* type, GLchar* name);
    void (*glGetTransformFeedbacki_v)(GLuint xfb, GLenum pname, GLuint index, GLint* param);
    void (*glGetTransformFeedbacki64_v)(GLuint xfb, GLenum pname, GLuint index, GLint64* param);
    // P3: shader / program binaries (GL 4.1)
    void (*glGetProgramBinary)(GLuint program, GLsizei bufSize, GLsizei* length, GLenum* binaryFormat, void* binary);
    void (*glProgramBinary)(GLuint program, GLenum binaryFormat, const void* binary, GLsizei length);
    void (*glShaderBinary)(GLsizei count, const GLuint* shaders, GLenum binaryFormat, const void* binary, GLsizei length);
    void (*glReleaseShaderCompiler)();
    void (*glGetShaderPrecisionFormat)(GLenum shaderType, GLenum precisionType, GLint* range, GLint* precision);
    // P3: subroutines (GL 4.0)
    GLint (*glGetSubroutineUniformLocation)(GLuint program, GLenum shaderType, const GLchar* name);
    GLuint (*glGetSubroutineIndex)(GLuint program, GLenum shaderType, const GLchar* name);
    void (*glUniformSubroutinesuiv)(GLenum shaderType, GLsizei count, const GLuint* indices);
    void (*glGetUniformSubroutineuiv)(GLenum shaderType, GLint location, GLuint* params);
    void (*glGetActiveSubroutineUniformiv)(GLuint program, GLenum shaderType, GLuint index, GLenum pname, GLint* values);
    void (*glGetActiveSubroutineUniformName)(GLuint program, GLenum shaderType, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name);
    void (*glGetActiveSubroutineName)(GLuint program, GLenum shaderType, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name);
    // P3: GL 4.6
    void (*glMultiDrawArraysIndirectCount)(GLenum mode, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride);
    void (*glMultiDrawElementsIndirectCount)(GLenum mode, GLenum type, const void* indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride);
    void (*glPolygonOffsetClamp)(GLfloat factor, GLfloat units, GLfloat clamp);
    void (*glSpecializeShader)(GLuint shader, const GLchar* pEntryPoint, GLuint numSpecializationConstants, const GLuint* pConstantIndex, const GLuint* pConstantValue);
    // F22: reflection — active uniforms/attributes, uniform blocks, program resources (GL 4.3)
    void (*glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
    void (*glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
    void (*glGetActiveUniformName)(GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformName);
    void (*glGetActiveUniformsiv)(GLuint program, GLsizei uniformCount, const GLuint* uniformIndices, GLenum pname, GLint* params);
    void (*glGetActiveUniformBlockiv)(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params);
    void (*glGetActiveUniformBlockName)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName);
    void (*glGetUniformIndices)(GLuint program, GLsizei uniformCount, const GLchar* const* uniformNames, GLuint* uniformIndices);
    void (*glGetProgramInterfaceiv)(GLuint program, GLenum programInterface, GLenum pname, GLint* params);
    void (*glGetProgramResourceiv)(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum* props, GLsizei count, GLsizei* length, GLint* params);
    void (*glGetProgramResourceName)(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei* length, GLchar* name);
    GLuint (*glGetProgramResourceIndex)(GLuint program, GLenum programInterface, const GLchar* name);
    void (*glGetAttachedShaders)(GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders);
};

/// @brief Direct State Access entry points (GL 4.5). Filled by init() via procAddress;
/// every member is nullptr when the driver/context does not support DSA.
struct DsaFunctions {
    void (*glCreateBuffers)(GLsizei n, GLuint* buffers);
    void (*glNamedBufferData)(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage);
    void (*glNamedBufferSubData)(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data);
    void (*glCreateTextures)(GLenum target, GLsizei n, GLuint* textures);
    void (*glTextureStorage2D)(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
    void (*glTextureStorage3D)(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
    void (*glTextureSubImage2D)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
    void (*glCreateSamplers)(GLsizei n, GLuint* samplers);
    void (*glSamplerParameteri)(GLuint sampler, GLenum pname, GLint param);
    void (*glBindSampler)(GLuint unit, GLuint sampler);
    void (*glBindTextureUnit)(GLuint unit, GLuint texture);
    void (*glCreateVertexArrays)(GLsizei n, GLuint* arrays);
    void (*glEnableVertexArrayAttrib)(GLuint vaobj, GLuint index);
    void (*glVertexArrayAttribFormat)(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
    void (*glVertexArrayVertexBuffer)(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
    void (*glVertexArrayElementBuffer)(GLuint vaobj, GLuint buffer);
    void (*glCreateFramebuffers)(GLsizei n, GLuint* framebuffers);
    void (*glNamedFramebufferTexture)(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
    GLenum (*glCheckNamedFramebufferStatus)(GLuint framebuffer, GLenum target);
    void (*glCreateRenderbuffers)(GLsizei n, GLuint* renderbuffers);
    void (*glNamedRenderbufferStorage)(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
    void (*glBlitNamedFramebuffer)(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
    // buffers (P2): immutable storage + mapping + copies + clears + queries
    void (*glNamedBufferStorage)(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags);
    void* (*glMapNamedBuffer)(GLuint buffer, GLenum access);
    void* (*glMapNamedBufferRange)(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access);
    GLboolean (*glUnmapNamedBuffer)(GLuint buffer);
    void (*glFlushMappedNamedBufferRange)(GLuint buffer, GLintptr offset, GLsizeiptr length);
    void (*glCopyNamedBufferSubData)(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
    void (*glClearNamedBufferSubData)(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data);
    void (*glInvalidateNamedBufferData)(GLuint buffer);
    void (*glInvalidateNamedBufferSubData)(GLuint buffer, GLintptr offset, GLsizeiptr length);
    void (*glGetNamedBufferParameteriv)(GLuint buffer, GLenum pname, GLint* params);
    void (*glGetNamedBufferParameteri64v)(GLuint buffer, GLenum pname, GLint64* params);
    void (*glGetNamedBufferSubData)(GLuint buffer, GLintptr offset, GLsizeiptr size, void* data);
    // transform feedback (P2)
    void (*glCreateTransformFeedbacks)(GLsizei n, GLuint* ids);
    // framebuffers (P2): draw/read buffers, typed clears, texture layers
    void (*glNamedFramebufferDrawBuffer)(GLuint framebuffer, GLenum buf);
    void (*glNamedFramebufferDrawBuffers)(GLuint framebuffer, GLsizei n, const GLenum* bufs);
    void (*glNamedFramebufferReadBuffer)(GLuint framebuffer, GLenum src);
    void (*glClearNamedFramebufferiv)(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint* value);
    void (*glClearNamedFramebufferuiv)(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint* value);
    void (*glClearNamedFramebufferfv)(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat* value);
    void (*glClearNamedFramebufferfi)(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
    void (*glNamedFramebufferTextureLayer)(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer);
    // renderbuffers (P2): multisample storage + params
    void (*glNamedRenderbufferStorageMultisample)(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
    void (*glGetNamedRenderbufferParameteriv)(GLuint renderbuffer, GLenum pname, GLint* params);
    // textures (P2): params, 3D/compressed/copy uploads, mipmaps, MS storage, readback, buffer textures
    void (*glTextureParameteri)(GLuint texture, GLenum pname, GLint param);
    void (*glTextureParameterf)(GLuint texture, GLenum pname, GLfloat param);
    void (*glTextureParameterfv)(GLuint texture, GLenum pname, const GLfloat* params);
    void (*glTextureSubImage3D)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
    void (*glCompressedTextureSubImage2D)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data);
    void (*glCompressedTextureSubImage3D)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data);
    void (*glCopyTextureSubImage2D)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glCopyTextureSubImage3D)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glGenerateTextureMipmap)(GLuint texture);
    void (*glTextureStorage2DMultisample)(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
    void (*glTextureStorage3DMultisample)(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations);
    void (*glGetTextureImage)(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels);
    void (*glGetCompressedTextureImage)(GLuint texture, GLint level, GLsizei bufSize, void* pixels);
    void (*glGetTextureLevelParameteriv)(GLuint texture, GLint level, GLenum pname, GLint* params);
    void (*glTextureBuffer)(GLuint texture, GLenum internalformat, GLuint buffer);
    void (*glTextureBufferRange)(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size);
    // vertex arrays (P2): attrib binding, integer/double formats, divisors, queries
    void (*glVertexArrayAttribBinding)(GLuint vaobj, GLuint attribindex, GLuint bindingindex);
    void (*glVertexArrayAttribIFormat)(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
    void (*glVertexArrayAttribLFormat)(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset);
    void (*glVertexArrayBindingDivisor)(GLuint vaobj, GLuint bindingindex, GLuint divisor);
    void (*glGetVertexArrayiv)(GLuint vaobj, GLenum pname, GLint* param);
    void (*glGetVertexArrayIndexediv)(GLuint vaobj, GLuint index, GLenum pname, GLint* param);
    void (*glGetVertexArrayIndexed64iv)(GLuint vaobj, GLuint index, GLenum pname, GLint64* param);
    // samplers (P2): float params + range binding
    void (*glSamplerParameterf)(GLuint sampler, GLenum pname, GLfloat param);
    void (*glSamplerParameterfv)(GLuint sampler, GLenum pname, const GLfloat* params);
    void (*glBindSamplers)(GLuint first, GLsizei count, const GLuint* samplers);
    // queries (P2): DSA creation (glBeginQuery/glGetQueryObjectui64v live in the curated table)
    void (*glCreateQueries)(GLenum target, GLsizei n, GLuint* ids);
};

/// @brief DSA entry points; all null if unavailable (use hasDsa() first).
const DsaFunctions& dsa();
/// @brief True when the current context is GL >= 4.5 and DSA pointers were found.
bool hasDsa();

/// @brief Names of all entries in the DSA table (for diagnostics/tests).
const char* const* dsaFunctionNames();
/// @brief Number of entries in the DSA table.
std::size_t dsaFunctionNameCount();

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
