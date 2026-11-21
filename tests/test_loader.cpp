#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/gl/loader.hpp>

#include <cstring>
#include <set>
#include <string>

using namespace fuhgl::gl;

void runLoaderTests() {
    // parseVersion
    CHECK((parseVersion("4.6.0 NVIDIA 535.54.03") == Version{4, 6}));
    CHECK((parseVersion("3.3.0 Mesa 23.0.4") == Version{3, 3}));
    CHECK((parseVersion("2.1.2") == Version{2, 1}));
    CHECK((parseVersion("garbage") == Version{0, 0}));
    CHECK((parseVersion(nullptr) == Version{0, 0}));
    CHECK((Version{4, 6}.atLeast(3, 3)));
    CHECK((Version{4, 6}.atLeast(4, 6)));
    CHECK(!(Version{3, 3}.atLeast(4, 0)));

    // splitExtensions
    const auto ext = splitExtensions("GL_ARB_texture GL_EXT_framebuffer_object  GL_KHR_debug ");
    CHECK(ext.size() == 3);
    CHECK(ext[0] == "GL_ARB_texture");
    CHECK(ext[2] == "GL_KHR_debug");
    CHECK(splitExtensions("").empty());
    CHECK(splitExtensions(nullptr).empty());

    // curated function table: non-empty, unique, all gl-prefixed
    CHECK(functionNameCount() > 50);
    std::set<std::string> names;
    for (std::size_t i = 0; i < functionNameCount(); ++i) {
        const char* n = functionNames()[i];
        CHECK(n != nullptr);
        CHECK(std::strncmp(n, "gl", 2) == 0);
        names.insert(n);
    }
    CHECK(names.size() == functionNameCount()); // no duplicates

    // F9 entries (buffer/texture/image power-ups) must be in the curated table
    const char* f9[] = {
        "glMapBufferRange", "glFlushMappedBufferRange", "glCopyBufferSubData", "glGetBufferSubData",
        "glGetBufferParameteriv", "glGetBufferParameteri64v", "glInvalidateBufferData", "glInvalidateBufferSubData",
        "glVertexAttribIPointer", "glVertexAttribLPointer", "glBindVertexBuffer", "glBindVertexBuffers",
        "glVertexAttribBinding", "glVertexAttribFormat", "glVertexAttribIFormat", "glVertexAttribLFormat",
        "glVertexBindingDivisor", "glTexSubImage3D", "glCopyTexImage2D", "glCopyTexSubImage2D", "glCopyTexSubImage3D",
        "glGetTexImage", "glGetCompressedTexImage", "glGetTexLevelParameteriv", "glGetTexParameteriv", "glGetTexParameterfv",
        "glCompressedTexImage2D", "glCompressedTexImage3D", "glCompressedTexSubImage2D", "glCompressedTexSubImage3D",
        "glTexBuffer", "glTexBufferRange", "glTextureView", "glBindImageTexture", "glBindImageTextures",
        "glDispatchComputeIndirect", "glShaderStorageBlockBinding", "glGetShaderStorageBlockIndex",
        "glGetUniformiv", "glGetUniformfv", "glGetUniformuiv", "glUniform1ui", "glUniform4uiv",
    };
    for (const char* fn : f9) CHECK(names.count(fn) == 1);

    // F13 entries (state/draw/debug/queries/getters/images) must be in the table
    const char* f13[] = {
        "glDrawBuffer", "glFramebufferTexture", "glFramebufferTextureLayer", "glClearBufferuiv", "glClearBufferfi",
        "glInvalidateFramebuffer", "glInvalidateSubFramebuffer", "glGetFramebufferAttachmentParameteriv",
        "glBlendEquation", "glBlendEquationSeparate", "glBlendFuncSeparate", "glBlendColor", "glLogicOp",
        "glEnablei", "glDisablei", "glIsEnabledi", "glBlendEquationi", "glBlendFunci", "glColorMaski",
        "glStencilFunc", "glStencilFuncSeparate", "glStencilOp", "glStencilOpSeparate", "glStencilMask", "glStencilMaskSeparate",
        "glPolygonMode", "glPolygonOffset", "glLineWidth", "glPointSize", "glSampleCoverage", "glSampleMaski",
        "glMinSampleShading", "glDepthRange", "glDepthRangef", "glDepthRangeArrayv", "glDepthRangeIndexed",
        "glViewportArrayv", "glViewportIndexedf", "glScissorArrayv", "glScissorIndexed",
        "glProvokingVertex", "glPrimitiveRestartIndex", "glHint", "glPatchParameteri", "glPatchParameterfv",
        "glDrawRangeElements", "glDrawElementsBaseVertex", "glDrawRangeElementsBaseVertex",
        "glDrawElementsInstancedBaseVertex", "glDrawArraysInstancedBaseInstance", "glDrawElementsInstancedBaseInstance",
        "glDrawElementsInstancedBaseVertexBaseInstance", "glMultiDrawArrays", "glMultiDrawElements",
        "glMultiDrawElementsBaseVertex", "glMultiDrawArraysIndirect", "glMultiDrawElementsIndirect",
        "glGetQueryiv", "glBeginQueryIndexed", "glEndQueryIndexed", "glWaitSync", "glGetSynciv",
        "glGetBooleanv", "glGetBooleani_v", "glGetIntegeri_v", "glGetInteger64i_v",
        "glIsEnabled", "glIsBuffer", "glIsTexture", "glIsVertexArray", "glIsFramebuffer", "glIsRenderbuffer",
        "glIsProgram", "glIsShader", "glIsSync", "glIsQuery", "glIsSampler",
        "glDebugMessageControl", "glDebugMessageInsert", "glGetDebugMessageLog", "glPushDebugGroup", "glPopDebugGroup",
        "glObjectLabel", "glGetObjectLabel", "glObjectPtrLabel", "glGetObjectPtrLabel",
        "glGetInternalformativ", "glGetInternalformati64v", "glCopyImageSubData", "glClearTexImage", "glClearTexSubImage",
    };
    for (const char* fn : f13) CHECK(names.count(fn) == 1);

    // DSA table (GL 4.5): names are known even without a GL context; all entries
    // registered exactly once, all gl-prefixed. P2 completed the table to 72 entries.
    CHECK(dsaFunctionNameCount() == 72);
    std::set<std::string> dsaNames;
    for (std::size_t i = 0; i < dsaFunctionNameCount(); ++i) {
        const char* n = dsaFunctionNames()[i];
        CHECK(n != nullptr);
        CHECK(std::strncmp(n, "gl", 2) == 0);
        dsaNames.insert(n);
    }
    CHECK(dsaNames.size() == dsaFunctionNameCount()); // no duplicates
    const char* p2[] = {
        "glNamedBufferStorage", "glMapNamedBuffer", "glMapNamedBufferRange", "glUnmapNamedBuffer",
        "glFlushMappedNamedBufferRange", "glCopyNamedBufferSubData", "glClearNamedBufferSubData",
        "glInvalidateNamedBufferData", "glInvalidateNamedBufferSubData", "glGetNamedBufferParameteriv",
        "glGetNamedBufferParameteri64v", "glGetNamedBufferSubData", "glCreateTransformFeedbacks",
        "glNamedFramebufferDrawBuffer", "glNamedFramebufferDrawBuffers", "glNamedFramebufferReadBuffer",
        "glClearNamedFramebufferiv", "glClearNamedFramebufferuiv", "glClearNamedFramebufferfv",
        "glClearNamedFramebufferfi", "glNamedFramebufferTextureLayer",
        "glNamedRenderbufferStorageMultisample", "glGetNamedRenderbufferParameteriv",
        "glTextureParameteri", "glTextureParameterf", "glTextureParameterfv", "glTextureSubImage3D",
        "glCompressedTextureSubImage2D", "glCompressedTextureSubImage3D", "glCopyTextureSubImage2D",
        "glCopyTextureSubImage3D", "glGenerateTextureMipmap", "glTextureStorage2DMultisample",
        "glTextureStorage3DMultisample", "glGetTextureImage", "glGetCompressedTextureImage",
        "glGetTextureLevelParameteriv", "glTextureBuffer", "glTextureBufferRange",
        "glVertexArrayAttribBinding", "glVertexArrayAttribIFormat", "glVertexArrayAttribLFormat",
        "glVertexArrayBindingDivisor", "glGetVertexArrayiv", "glGetVertexArrayIndexediv",
        "glGetVertexArrayIndexed64iv", "glSamplerParameterf", "glSamplerParameterfv", "glBindSamplers",
        "glCreateQueries",
    };
    for (const char* fn : p2) CHECK(dsaNames.count(fn) == 1);

    // P3 entries (program pipelines, transform feedback, binaries, subroutines, GL 4.6)
    const char* f21[] = {
        "glCreateProgramPipelines", "glDeleteProgramPipelines", "glIsProgramPipeline", "glBindProgramPipeline",
        "glUseProgramStages", "glActiveShaderProgram", "glValidateProgramPipeline", "glGetProgramPipelineiv",
        "glGetProgramPipelineInfoLog", "glCreateShaderProgramv", "glProgramParameteri",
        "glProgramUniform1i", "glProgramUniform4i", "glProgramUniform1f", "glProgramUniform4f",
        "glProgramUniform1ui", "glProgramUniform4ui", "glProgramUniform1iv", "glProgramUniform4fv",
        "glProgramUniform1uiv", "glProgramUniformMatrix4fv", "glProgramUniformMatrix2x3fv", "glProgramUniformMatrix4x3fv",
        "glGenTransformFeedbacks", "glDeleteTransformFeedbacks", "glIsTransformFeedback", "glBindTransformFeedback",
        "glBeginTransformFeedback", "glEndTransformFeedback", "glPauseTransformFeedback", "glResumeTransformFeedback",
        "glDrawTransformFeedback", "glDrawTransformFeedbackStream", "glTransformFeedbackVaryings",
        "glGetTransformFeedbackVarying", "glGetTransformFeedbacki_v", "glGetTransformFeedbacki64_v",
        "glGetProgramBinary", "glProgramBinary", "glShaderBinary", "glReleaseShaderCompiler", "glGetShaderPrecisionFormat",
        "glGetSubroutineUniformLocation", "glGetSubroutineIndex", "glUniformSubroutinesuiv", "glGetUniformSubroutineuiv",
        "glGetActiveSubroutineUniformiv", "glGetActiveSubroutineUniformName", "glGetActiveSubroutineName",
        "glMultiDrawArraysIndirectCount", "glMultiDrawElementsIndirectCount", "glPolygonOffsetClamp", "glSpecializeShader",
    };
    for (const char* fn : f21) CHECK(names.count(fn) == 1);

    // F22 reflection entries must be in the table
    const char* f22[] = {
        "glGetActiveAttrib", "glGetActiveUniform", "glGetActiveUniformName", "glGetActiveUniformsiv",
        "glGetActiveUniformBlockiv", "glGetActiveUniformBlockName", "glGetUniformIndices",
        "glGetProgramInterfaceiv", "glGetProgramResourceiv", "glGetProgramResourceName",
        "glGetProgramResourceIndex", "glGetAttachedShaders",
    };
    for (const char* fn : f22) CHECK(names.count(fn) == 1);

    // procAddress contract: nullptr in -> nullptr out. (We must NOT assert nullptr
    // for unknown names: Mesa's glXGetProcAddress returns no-op stubs for those.)
    CHECK(procAddress(nullptr) == nullptr);
}
