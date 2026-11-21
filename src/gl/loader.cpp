// Runtime OpenGL function loader. Zero dependencies: the GL library is opened
// at runtime (dlopen on Linux/macOS, LoadLibrary on Windows), so no OpenGL dev
// packages or third-party loaders are needed at build time.

#include "fuhgl/gl/loader.hpp"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace fuhgl::gl {
namespace {

Functions g_functions{};
bool g_initialized = false;
void* g_lib = nullptr; // handle of the platform GL library

#if defined(_WIN32)

void* openGlLibrary() {
    if (HMODULE h = LoadLibraryA("opengl32.dll")) return static_cast<void*>(h);
    return nullptr;
}

void* lookupProc(const char* name) {
    if (!g_lib) return nullptr;
    // wglGetProcAddress covers everything above GL 1.1 (and wgl* itself);
    // GL 1.1 entry points live directly in opengl32.dll.
    using WglGetProc = void* (__stdcall*)(const char*);
    if (auto wglGetProcAddress = reinterpret_cast<WglGetProc>(
            GetProcAddress(static_cast<HMODULE>(g_lib), "wglGetProcAddress"))) {
        if (void* p = wglGetProcAddress(name)) return p;
    }
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(g_lib), name));
}

#elif defined(__APPLE__)

void* openGlLibrary() {
    if (void* h = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_GLOBAL))
        return h;
    return dlopen("libGL.dylib", RTLD_LAZY | RTLD_GLOBAL);
}

void* lookupProc(const char* name) {
    if (!g_lib) return nullptr;
    if (void* p = dlsym(g_lib, name)) return p;
    return dlsym(RTLD_DEFAULT, name); // symbols re-exported by other loaded GL libs
}

#else // Linux / other Unix: GLX via libGL

void* openGlLibrary() {
    if (void* h = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL)) return h;
    return dlopen("libGL.so", RTLD_LAZY | RTLD_GLOBAL);
}

void* lookupProc(const char* name) {
    if (!g_lib) return nullptr;
    using GlxGetProc = void* (*)(const unsigned char*);
    if (auto glXGetProcAddress = reinterpret_cast<GlxGetProc>(dlsym(g_lib, "glXGetProcAddressARB"))) {
        if (void* p = glXGetProcAddress(reinterpret_cast<const unsigned char*>(name))) return p;
    }
    if (auto glXGetProcAddress = reinterpret_cast<GlxGetProc>(dlsym(g_lib, "glXGetProcAddress"))) {
        if (void* p = glXGetProcAddress(reinterpret_cast<const unsigned char*>(name))) return p;
    }
    if (void* p = dlsym(g_lib, name)) return p; // GL 1.0/1.1 entry points
    return dlsym(RTLD_DEFAULT, name); // e.g. libGLdispatch/libEGL loaded globally (EGL-only setups)
}

#endif

struct Entry {
    const char* name;
    void (*set)(Functions&, void*);
};

#define FUHGL_ENTRY(fn)                                                          \
    { #fn, [](Functions& f, void* p) { f.fn = reinterpret_cast<decltype(f.fn)>(p); } }

const Entry kEntries[] = {
    FUHGL_ENTRY(glGetError), FUHGL_ENTRY(glGetString), FUHGL_ENTRY(glGetStringi), FUHGL_ENTRY(glGetIntegerv),
    FUHGL_ENTRY(glGetInteger64v),
    FUHGL_ENTRY(glEnable), FUHGL_ENTRY(glDisable), FUHGL_ENTRY(glClear), FUHGL_ENTRY(glClearColor),
    FUHGL_ENTRY(glViewport), FUHGL_ENTRY(glScissor), FUHGL_ENTRY(glBlendFunc), FUHGL_ENTRY(glDepthFunc),
    FUHGL_ENTRY(glDepthMask), FUHGL_ENTRY(glColorMask), FUHGL_ENTRY(glCullFace), FUHGL_ENTRY(glFrontFace),
    FUHGL_ENTRY(glPixelStorei), FUHGL_ENTRY(glFinish), FUHGL_ENTRY(glFlush),
    FUHGL_ENTRY(glCreateShader), FUHGL_ENTRY(glShaderSource), FUHGL_ENTRY(glCompileShader),
    FUHGL_ENTRY(glGetShaderiv), FUHGL_ENTRY(glGetShaderInfoLog), FUHGL_ENTRY(glDeleteShader),
    FUHGL_ENTRY(glCreateProgram), FUHGL_ENTRY(glAttachShader), FUHGL_ENTRY(glLinkProgram),
    FUHGL_ENTRY(glGetProgramiv), FUHGL_ENTRY(glGetProgramInfoLog), FUHGL_ENTRY(glUseProgram),
    FUHGL_ENTRY(glDeleteProgram), FUHGL_ENTRY(glGetUniformLocation), FUHGL_ENTRY(glGetAttribLocation),
    FUHGL_ENTRY(glBindAttribLocation), FUHGL_ENTRY(glUniform1i), FUHGL_ENTRY(glUniform1f),
    FUHGL_ENTRY(glUniform2f), FUHGL_ENTRY(glUniform3f), FUHGL_ENTRY(glUniform4f), FUHGL_ENTRY(glUniform1iv),
    FUHGL_ENTRY(glUniformMatrix3fv), FUHGL_ENTRY(glUniformMatrix4fv), FUHGL_ENTRY(glUniformBlockBinding),
    FUHGL_ENTRY(glGetUniformBlockIndex),
    FUHGL_ENTRY(glGetUniformiv), FUHGL_ENTRY(glGetUniformfv), FUHGL_ENTRY(glGetUniformuiv),
    FUHGL_ENTRY(glUniform1ui), FUHGL_ENTRY(glUniform2ui), FUHGL_ENTRY(glUniform3ui), FUHGL_ENTRY(glUniform4ui),
    FUHGL_ENTRY(glUniform1uiv), FUHGL_ENTRY(glUniform2uiv), FUHGL_ENTRY(glUniform3uiv), FUHGL_ENTRY(glUniform4uiv),
    FUHGL_ENTRY(glGetShaderStorageBlockIndex), FUHGL_ENTRY(glShaderStorageBlockBinding),
    FUHGL_ENTRY(glGenBuffers), FUHGL_ENTRY(glBindBuffer), FUHGL_ENTRY(glBufferData), FUHGL_ENTRY(glBufferSubData),
    FUHGL_ENTRY(glDeleteBuffers), FUHGL_ENTRY(glMapBuffer), FUHGL_ENTRY(glMapBufferRange), FUHGL_ENTRY(glUnmapBuffer),
    FUHGL_ENTRY(glFlushMappedBufferRange), FUHGL_ENTRY(glCopyBufferSubData), FUHGL_ENTRY(glGetBufferSubData),
    FUHGL_ENTRY(glGetBufferParameteriv), FUHGL_ENTRY(glGetBufferParameteri64v),
    FUHGL_ENTRY(glInvalidateBufferData), FUHGL_ENTRY(glInvalidateBufferSubData),
    FUHGL_ENTRY(glBindBufferBase), FUHGL_ENTRY(glBindBufferRange),
    FUHGL_ENTRY(glGenVertexArrays), FUHGL_ENTRY(glBindVertexArray), FUHGL_ENTRY(glDeleteVertexArrays),
    FUHGL_ENTRY(glEnableVertexAttribArray), FUHGL_ENTRY(glDisableVertexAttribArray),
    FUHGL_ENTRY(glVertexAttribPointer), FUHGL_ENTRY(glVertexAttribIPointer), FUHGL_ENTRY(glVertexAttribLPointer),
    FUHGL_ENTRY(glVertexAttribDivisor),
    FUHGL_ENTRY(glBindVertexBuffer), FUHGL_ENTRY(glBindVertexBuffers), FUHGL_ENTRY(glVertexAttribBinding),
    FUHGL_ENTRY(glVertexAttribFormat), FUHGL_ENTRY(glVertexAttribIFormat), FUHGL_ENTRY(glVertexAttribLFormat),
    FUHGL_ENTRY(glVertexBindingDivisor),
    FUHGL_ENTRY(glGenTextures), FUHGL_ENTRY(glBindTexture), FUHGL_ENTRY(glDeleteTextures), FUHGL_ENTRY(glActiveTexture),
    FUHGL_ENTRY(glTexImage2D), FUHGL_ENTRY(glTexSubImage2D), FUHGL_ENTRY(glTexSubImage3D),
    FUHGL_ENTRY(glCopyTexImage2D), FUHGL_ENTRY(glCopyTexSubImage2D), FUHGL_ENTRY(glCopyTexSubImage3D),
    FUHGL_ENTRY(glGetTexImage), FUHGL_ENTRY(glGetCompressedTexImage),
    FUHGL_ENTRY(glGetTexLevelParameteriv), FUHGL_ENTRY(glGetTexParameteriv), FUHGL_ENTRY(glGetTexParameterfv),
    FUHGL_ENTRY(glCompressedTexImage2D), FUHGL_ENTRY(glCompressedTexImage3D),
    FUHGL_ENTRY(glCompressedTexSubImage2D), FUHGL_ENTRY(glCompressedTexSubImage3D),
    FUHGL_ENTRY(glTexBuffer), FUHGL_ENTRY(glTexBufferRange), FUHGL_ENTRY(glTextureView),
    FUHGL_ENTRY(glGenerateMipmap),
    FUHGL_ENTRY(glTexParameteri), FUHGL_ENTRY(glTexParameterf),
    FUHGL_ENTRY(glGenFramebuffers), FUHGL_ENTRY(glBindFramebuffer), FUHGL_ENTRY(glDeleteFramebuffers),
    FUHGL_ENTRY(glFramebufferTexture2D), FUHGL_ENTRY(glCheckFramebufferStatus), FUHGL_ENTRY(glDrawBuffers),
    FUHGL_ENTRY(glBlitFramebuffer),
    FUHGL_ENTRY(glGenRenderbuffers), FUHGL_ENTRY(glBindRenderbuffer), FUHGL_ENTRY(glRenderbufferStorage),
    FUHGL_ENTRY(glFramebufferRenderbuffer), FUHGL_ENTRY(glDeleteRenderbuffers),
    FUHGL_ENTRY(glDrawArrays), FUHGL_ENTRY(glDrawElements), FUHGL_ENTRY(glDrawArraysInstanced),
    FUHGL_ENTRY(glDrawElementsInstanced),
    FUHGL_ENTRY(glDebugMessageCallback),
    FUHGL_ENTRY(glReadPixels), FUHGL_ENTRY(glReadBuffer), FUHGL_ENTRY(glClearBufferfv), FUHGL_ENTRY(glClearBufferiv),
    FUHGL_ENTRY(glDispatchCompute), FUHGL_ENTRY(glDispatchComputeIndirect), FUHGL_ENTRY(glMemoryBarrier),
    FUHGL_ENTRY(glBindImageTexture), FUHGL_ENTRY(glBindImageTextures),
    FUHGL_ENTRY(glGenQueries), FUHGL_ENTRY(glDeleteQueries), FUHGL_ENTRY(glBeginQuery), FUHGL_ENTRY(glEndQuery),
    FUHGL_ENTRY(glGetQueryObjectui64v), FUHGL_ENTRY(glQueryCounter),
    FUHGL_ENTRY(glFenceSync), FUHGL_ENTRY(glDeleteSync), FUHGL_ENTRY(glClientWaitSync),
    FUHGL_ENTRY(glDrawArraysIndirect), FUHGL_ENTRY(glDrawElementsIndirect),
    FUHGL_ENTRY(glGenSamplers), FUHGL_ENTRY(glDeleteSamplers), FUHGL_ENTRY(glBindSampler),
    FUHGL_ENTRY(glSamplerParameteri), FUHGL_ENTRY(glSamplerParameterf),
    FUHGL_ENTRY(glTexStorage2D), FUHGL_ENTRY(glTexStorage3D), FUHGL_ENTRY(glTexStorage2DMultisample),
    FUHGL_ENTRY(glRenderbufferStorageMultisample),
    // F13: framebuffers
    FUHGL_ENTRY(glDrawBuffer), FUHGL_ENTRY(glFramebufferTexture), FUHGL_ENTRY(glFramebufferTextureLayer),
    FUHGL_ENTRY(glClearBufferuiv), FUHGL_ENTRY(glClearBufferfi),
    FUHGL_ENTRY(glInvalidateFramebuffer), FUHGL_ENTRY(glInvalidateSubFramebuffer),
    FUHGL_ENTRY(glGetFramebufferAttachmentParameteriv),
    // F13: blending / logic / stencil
    FUHGL_ENTRY(glBlendEquation), FUHGL_ENTRY(glBlendEquationSeparate), FUHGL_ENTRY(glBlendFuncSeparate),
    FUHGL_ENTRY(glBlendColor), FUHGL_ENTRY(glLogicOp), FUHGL_ENTRY(glEnablei), FUHGL_ENTRY(glDisablei),
    FUHGL_ENTRY(glIsEnabledi), FUHGL_ENTRY(glBlendEquationi), FUHGL_ENTRY(glBlendEquationSeparatei),
    FUHGL_ENTRY(glBlendFunci), FUHGL_ENTRY(glBlendFuncSeparatei), FUHGL_ENTRY(glColorMaski),
    FUHGL_ENTRY(glStencilFunc), FUHGL_ENTRY(glStencilFuncSeparate), FUHGL_ENTRY(glStencilOp),
    FUHGL_ENTRY(glStencilOpSeparate), FUHGL_ENTRY(glStencilMask), FUHGL_ENTRY(glStencilMaskSeparate),
    // F13: rasterizer / viewport / scissor
    FUHGL_ENTRY(glPolygonMode), FUHGL_ENTRY(glPolygonOffset), FUHGL_ENTRY(glLineWidth), FUHGL_ENTRY(glPointSize),
    FUHGL_ENTRY(glPointParameterf), FUHGL_ENTRY(glPointParameterfv), FUHGL_ENTRY(glSampleCoverage),
    FUHGL_ENTRY(glSampleMaski), FUHGL_ENTRY(glMinSampleShading), FUHGL_ENTRY(glDepthRange), FUHGL_ENTRY(glDepthRangef),
    FUHGL_ENTRY(glDepthRangeArrayv), FUHGL_ENTRY(glDepthRangeIndexed), FUHGL_ENTRY(glViewportArrayv),
    FUHGL_ENTRY(glViewportIndexedf), FUHGL_ENTRY(glScissorArrayv), FUHGL_ENTRY(glScissorIndexed),
    FUHGL_ENTRY(glProvokingVertex), FUHGL_ENTRY(glPrimitiveRestartIndex), FUHGL_ENTRY(glHint),
    // F13: patches + draw batching
    FUHGL_ENTRY(glPatchParameteri), FUHGL_ENTRY(glPatchParameterfv),
    FUHGL_ENTRY(glDrawRangeElements), FUHGL_ENTRY(glDrawElementsBaseVertex), FUHGL_ENTRY(glDrawRangeElementsBaseVertex),
    FUHGL_ENTRY(glDrawElementsInstancedBaseVertex), FUHGL_ENTRY(glDrawArraysInstancedBaseInstance),
    FUHGL_ENTRY(glDrawElementsInstancedBaseInstance), FUHGL_ENTRY(glDrawElementsInstancedBaseVertexBaseInstance),
    FUHGL_ENTRY(glMultiDrawArrays), FUHGL_ENTRY(glMultiDrawElements), FUHGL_ENTRY(glMultiDrawElementsBaseVertex),
    FUHGL_ENTRY(glMultiDrawArraysIndirect), FUHGL_ENTRY(glMultiDrawElementsIndirect),
    // F13: queries / sync / getters
    FUHGL_ENTRY(glGetQueryiv), FUHGL_ENTRY(glBeginQueryIndexed), FUHGL_ENTRY(glEndQueryIndexed),
    FUHGL_ENTRY(glWaitSync), FUHGL_ENTRY(glGetSynciv), FUHGL_ENTRY(glGetBooleanv), FUHGL_ENTRY(glGetBooleani_v),
    FUHGL_ENTRY(glGetIntegeri_v), FUHGL_ENTRY(glGetInteger64i_v),
    FUHGL_ENTRY(glIsEnabled), FUHGL_ENTRY(glIsBuffer), FUHGL_ENTRY(glIsTexture), FUHGL_ENTRY(glIsVertexArray),
    FUHGL_ENTRY(glIsFramebuffer), FUHGL_ENTRY(glIsRenderbuffer), FUHGL_ENTRY(glIsProgram), FUHGL_ENTRY(glIsShader),
    FUHGL_ENTRY(glIsSync), FUHGL_ENTRY(glIsQuery), FUHGL_ENTRY(glIsSampler),
    // F13: debug
    FUHGL_ENTRY(glDebugMessageControl), FUHGL_ENTRY(glDebugMessageInsert), FUHGL_ENTRY(glGetDebugMessageLog),
    FUHGL_ENTRY(glPushDebugGroup), FUHGL_ENTRY(glPopDebugGroup), FUHGL_ENTRY(glObjectLabel),
    FUHGL_ENTRY(glGetObjectLabel), FUHGL_ENTRY(glObjectPtrLabel), FUHGL_ENTRY(glGetObjectPtrLabel),
    // F13: internal formats / image ops
    FUHGL_ENTRY(glGetInternalformativ), FUHGL_ENTRY(glGetInternalformati64v),
    FUHGL_ENTRY(glCopyImageSubData), FUHGL_ENTRY(glClearTexImage), FUHGL_ENTRY(glClearTexSubImage),
    // P3: program pipelines / separable programs (GL 4.1)
    FUHGL_ENTRY(glCreateProgramPipelines), FUHGL_ENTRY(glDeleteProgramPipelines), FUHGL_ENTRY(glIsProgramPipeline),
    FUHGL_ENTRY(glBindProgramPipeline), FUHGL_ENTRY(glUseProgramStages), FUHGL_ENTRY(glActiveShaderProgram),
    FUHGL_ENTRY(glValidateProgramPipeline), FUHGL_ENTRY(glGetProgramPipelineiv), FUHGL_ENTRY(glGetProgramPipelineInfoLog),
    FUHGL_ENTRY(glCreateShaderProgramv), FUHGL_ENTRY(glProgramParameteri),
    FUHGL_ENTRY(glProgramUniform1i), FUHGL_ENTRY(glProgramUniform2i), FUHGL_ENTRY(glProgramUniform3i), FUHGL_ENTRY(glProgramUniform4i),
    FUHGL_ENTRY(glProgramUniform1f), FUHGL_ENTRY(glProgramUniform2f), FUHGL_ENTRY(glProgramUniform3f), FUHGL_ENTRY(glProgramUniform4f),
    FUHGL_ENTRY(glProgramUniform1ui), FUHGL_ENTRY(glProgramUniform2ui), FUHGL_ENTRY(glProgramUniform3ui), FUHGL_ENTRY(glProgramUniform4ui),
    FUHGL_ENTRY(glProgramUniform1iv), FUHGL_ENTRY(glProgramUniform2iv), FUHGL_ENTRY(glProgramUniform3iv), FUHGL_ENTRY(glProgramUniform4iv),
    FUHGL_ENTRY(glProgramUniform1fv), FUHGL_ENTRY(glProgramUniform2fv), FUHGL_ENTRY(glProgramUniform3fv), FUHGL_ENTRY(glProgramUniform4fv),
    FUHGL_ENTRY(glProgramUniform1uiv), FUHGL_ENTRY(glProgramUniform2uiv), FUHGL_ENTRY(glProgramUniform3uiv), FUHGL_ENTRY(glProgramUniform4uiv),
    FUHGL_ENTRY(glProgramUniformMatrix2fv), FUHGL_ENTRY(glProgramUniformMatrix3fv), FUHGL_ENTRY(glProgramUniformMatrix4fv),
    FUHGL_ENTRY(glProgramUniformMatrix2x3fv), FUHGL_ENTRY(glProgramUniformMatrix2x4fv), FUHGL_ENTRY(glProgramUniformMatrix3x2fv),
    FUHGL_ENTRY(glProgramUniformMatrix3x4fv), FUHGL_ENTRY(glProgramUniformMatrix4x2fv), FUHGL_ENTRY(glProgramUniformMatrix4x3fv),
    // P3: transform feedback (GL 4.0)
    FUHGL_ENTRY(glGenTransformFeedbacks), FUHGL_ENTRY(glDeleteTransformFeedbacks), FUHGL_ENTRY(glIsTransformFeedback),
    FUHGL_ENTRY(glBindTransformFeedback), FUHGL_ENTRY(glBeginTransformFeedback), FUHGL_ENTRY(glEndTransformFeedback),
    FUHGL_ENTRY(glPauseTransformFeedback), FUHGL_ENTRY(glResumeTransformFeedback),
    FUHGL_ENTRY(glDrawTransformFeedback), FUHGL_ENTRY(glDrawTransformFeedbackStream),
    FUHGL_ENTRY(glTransformFeedbackVaryings), FUHGL_ENTRY(glGetTransformFeedbackVarying),
    FUHGL_ENTRY(glGetTransformFeedbacki_v), FUHGL_ENTRY(glGetTransformFeedbacki64_v),
    // P3: shader / program binaries (GL 4.1)
    FUHGL_ENTRY(glGetProgramBinary), FUHGL_ENTRY(glProgramBinary), FUHGL_ENTRY(glShaderBinary),
    FUHGL_ENTRY(glReleaseShaderCompiler), FUHGL_ENTRY(glGetShaderPrecisionFormat),
    // P3: subroutines (GL 4.0)
    FUHGL_ENTRY(glGetSubroutineUniformLocation), FUHGL_ENTRY(glGetSubroutineIndex),
    FUHGL_ENTRY(glUniformSubroutinesuiv), FUHGL_ENTRY(glGetUniformSubroutineuiv),
    FUHGL_ENTRY(glGetActiveSubroutineUniformiv), FUHGL_ENTRY(glGetActiveSubroutineUniformName), FUHGL_ENTRY(glGetActiveSubroutineName),
    // P3: GL 4.6
    FUHGL_ENTRY(glMultiDrawArraysIndirectCount), FUHGL_ENTRY(glMultiDrawElementsIndirectCount),
    FUHGL_ENTRY(glPolygonOffsetClamp), FUHGL_ENTRY(glSpecializeShader),
};

// DSA (GL 4.5) — optional, resolved by name in init().
struct DsaEntry {
    const char* name;
    void (*set)(DsaFunctions&, void*);
};
#define FUHGL_DSA(fn) { #fn, [](DsaFunctions& d, void* p) { d.fn = reinterpret_cast<decltype(d.fn)>(p); } }
const DsaEntry kDsaEntries[] = {
    FUHGL_DSA(glCreateBuffers), FUHGL_DSA(glNamedBufferData), FUHGL_DSA(glNamedBufferSubData),
    FUHGL_DSA(glCreateTextures), FUHGL_DSA(glTextureStorage2D), FUHGL_DSA(glTextureStorage3D),
    FUHGL_DSA(glTextureSubImage2D), FUHGL_DSA(glCreateSamplers), FUHGL_DSA(glSamplerParameteri),
    FUHGL_DSA(glBindSampler), FUHGL_DSA(glBindTextureUnit), FUHGL_DSA(glCreateVertexArrays),
    FUHGL_DSA(glEnableVertexArrayAttrib), FUHGL_DSA(glVertexArrayAttribFormat),
    FUHGL_DSA(glVertexArrayVertexBuffer), FUHGL_DSA(glVertexArrayElementBuffer),
    FUHGL_DSA(glCreateFramebuffers), FUHGL_DSA(glNamedFramebufferTexture), FUHGL_DSA(glCheckNamedFramebufferStatus),
    FUHGL_DSA(glCreateRenderbuffers), FUHGL_DSA(glNamedRenderbufferStorage), FUHGL_DSA(glBlitNamedFramebuffer),
    // P2: buffers
    FUHGL_DSA(glNamedBufferStorage), FUHGL_DSA(glMapNamedBuffer), FUHGL_DSA(glMapNamedBufferRange),
    FUHGL_DSA(glUnmapNamedBuffer), FUHGL_DSA(glFlushMappedNamedBufferRange), FUHGL_DSA(glCopyNamedBufferSubData),
    FUHGL_DSA(glClearNamedBufferSubData), FUHGL_DSA(glInvalidateNamedBufferData), FUHGL_DSA(glInvalidateNamedBufferSubData),
    FUHGL_DSA(glGetNamedBufferParameteriv), FUHGL_DSA(glGetNamedBufferParameteri64v), FUHGL_DSA(glGetNamedBufferSubData),
    // P2: transform feedback
    FUHGL_DSA(glCreateTransformFeedbacks),
    // P2: framebuffers
    FUHGL_DSA(glNamedFramebufferDrawBuffer), FUHGL_DSA(glNamedFramebufferDrawBuffers), FUHGL_DSA(glNamedFramebufferReadBuffer),
    FUHGL_DSA(glClearNamedFramebufferiv), FUHGL_DSA(glClearNamedFramebufferuiv), FUHGL_DSA(glClearNamedFramebufferfv),
    FUHGL_DSA(glClearNamedFramebufferfi), FUHGL_DSA(glNamedFramebufferTextureLayer),
    // P2: renderbuffers
    FUHGL_DSA(glNamedRenderbufferStorageMultisample), FUHGL_DSA(glGetNamedRenderbufferParameteriv),
    // P2: textures
    FUHGL_DSA(glTextureParameteri), FUHGL_DSA(glTextureParameterf), FUHGL_DSA(glTextureParameterfv),
    FUHGL_DSA(glTextureSubImage3D), FUHGL_DSA(glCompressedTextureSubImage2D), FUHGL_DSA(glCompressedTextureSubImage3D),
    FUHGL_DSA(glCopyTextureSubImage2D), FUHGL_DSA(glCopyTextureSubImage3D), FUHGL_DSA(glGenerateTextureMipmap),
    FUHGL_DSA(glTextureStorage2DMultisample), FUHGL_DSA(glTextureStorage3DMultisample),
    FUHGL_DSA(glGetTextureImage), FUHGL_DSA(glGetCompressedTextureImage), FUHGL_DSA(glGetTextureLevelParameteriv),
    FUHGL_DSA(glTextureBuffer), FUHGL_DSA(glTextureBufferRange),
    // P2: vertex arrays
    FUHGL_DSA(glVertexArrayAttribBinding), FUHGL_DSA(glVertexArrayAttribIFormat), FUHGL_DSA(glVertexArrayAttribLFormat),
    FUHGL_DSA(glVertexArrayBindingDivisor), FUHGL_DSA(glGetVertexArrayiv), FUHGL_DSA(glGetVertexArrayIndexediv),
    FUHGL_DSA(glGetVertexArrayIndexed64iv),
    // P2: samplers + queries
    FUHGL_DSA(glSamplerParameterf), FUHGL_DSA(glSamplerParameterfv), FUHGL_DSA(glBindSamplers),
    FUHGL_DSA(glCreateQueries),
};
#undef FUHGL_DSA

DsaFunctions g_dsa{};
bool g_hasDsa = false;

} // namespace

const DsaFunctions& dsa() { return g_dsa; }

bool hasDsa() { return g_hasDsa; }

bool init() {
    if (g_initialized) return true;
    g_lib = openGlLibrary();
    if (!g_lib) return false;
    for (const Entry& e : kEntries)
        e.set(g_functions, lookupProc(e.name));
    for (const DsaEntry& e : kDsaEntries)
        e.set(g_dsa, lookupProc(e.name));
    g_hasDsa = version().atLeast(4, 5) && g_dsa.glCreateBuffers != nullptr;
    g_initialized = true;
    return true;
}

bool isInitialized() { return g_initialized; }

void* procAddress(const char* name) {
    if (!name) return nullptr;
    if (!g_lib) g_lib = openGlLibrary();
    if (!g_lib) return nullptr;
    return lookupProc(name);
}

const Functions& functions() { return g_functions; }

Version version() {
    if (g_functions.glGetString)
        if (const GLubyte* s = g_functions.glGetString(GL_VERSION))
            return parseVersion(reinterpret_cast<const char*>(s));
    return {};
}

std::vector<std::string> extensions() {
    std::vector<std::string> result;
    if (g_functions.glGetStringi && g_functions.glGetIntegerv) {
        GLint n = 0;
        g_functions.glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        for (GLint i = 0; i < n; ++i)
            if (const GLubyte* s = g_functions.glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)))
                result.emplace_back(reinterpret_cast<const char*>(s));
    } else if (g_functions.glGetString) {
        if (const GLubyte* s = g_functions.glGetString(GL_EXTENSIONS))
            result = splitExtensions(reinterpret_cast<const char*>(s));
    }
    return result;
}

bool hasExtension(const char* name) {
    for (const std::string& e : extensions())
        if (e == name) return true;
    return false;
}

Version parseVersion(const char* versionString) {
    Version v;
    if (!versionString) return v;
    if (std::sscanf(versionString, "%d.%d", &v.major, &v.minor) != 2) {
        v.major = v.minor = 0;
    }
    return v;
}

std::vector<std::string> splitExtensions(const char* extensionsString) {
    std::vector<std::string> result;
    if (!extensionsString) return result;
    const char* p = extensionsString;
    while (*p) {
        while (*p == ' ') ++p;
        const char* start = p;
        while (*p && *p != ' ') ++p;
        if (p > start) result.emplace_back(start, p);
    }
    return result;
}

const char* const* functionNames() {
    static const char* names[sizeof(kEntries) / sizeof(kEntries[0])];
    static bool filled = false;
    if (!filled) {
        for (std::size_t i = 0; i < std::size(kEntries); ++i) names[i] = kEntries[i].name;
        filled = true;
    }
    return names;
}

std::size_t functionNameCount() { return std::size(kEntries); }

const char* const* dsaFunctionNames() {
    static const char* names[sizeof(kDsaEntries) / sizeof(kDsaEntries[0])];
    static bool filled = false;
    if (!filled) {
        for (std::size_t i = 0; i < std::size(kDsaEntries); ++i) names[i] = kDsaEntries[i].name;
        filled = true;
    }
    return names;
}

std::size_t dsaFunctionNameCount() { return std::size(kDsaEntries); }

} // namespace fuhgl::gl
