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
    return dlsym(g_lib, name);
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
    return dlsym(g_lib, name); // GL 1.0/1.1 entry points
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
    FUHGL_ENTRY(glGenBuffers), FUHGL_ENTRY(glBindBuffer), FUHGL_ENTRY(glBufferData), FUHGL_ENTRY(glBufferSubData),
    FUHGL_ENTRY(glDeleteBuffers), FUHGL_ENTRY(glMapBuffer), FUHGL_ENTRY(glUnmapBuffer),
    FUHGL_ENTRY(glBindBufferBase), FUHGL_ENTRY(glBindBufferRange),
    FUHGL_ENTRY(glGenVertexArrays), FUHGL_ENTRY(glBindVertexArray), FUHGL_ENTRY(glDeleteVertexArrays),
    FUHGL_ENTRY(glEnableVertexAttribArray), FUHGL_ENTRY(glDisableVertexAttribArray),
    FUHGL_ENTRY(glVertexAttribPointer), FUHGL_ENTRY(glVertexAttribDivisor),
    FUHGL_ENTRY(glGenTextures), FUHGL_ENTRY(glBindTexture), FUHGL_ENTRY(glDeleteTextures), FUHGL_ENTRY(glActiveTexture),
    FUHGL_ENTRY(glTexImage2D), FUHGL_ENTRY(glTexSubImage2D), FUHGL_ENTRY(glGenerateMipmap),
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
    FUHGL_ENTRY(glDispatchCompute), FUHGL_ENTRY(glMemoryBarrier),
    FUHGL_ENTRY(glGenQueries), FUHGL_ENTRY(glDeleteQueries), FUHGL_ENTRY(glBeginQuery), FUHGL_ENTRY(glEndQuery),
    FUHGL_ENTRY(glGetQueryObjectui64v), FUHGL_ENTRY(glQueryCounter),
    FUHGL_ENTRY(glFenceSync), FUHGL_ENTRY(glDeleteSync), FUHGL_ENTRY(glClientWaitSync),
    FUHGL_ENTRY(glDrawArraysIndirect), FUHGL_ENTRY(glDrawElementsIndirect),
    FUHGL_ENTRY(glGenSamplers), FUHGL_ENTRY(glDeleteSamplers), FUHGL_ENTRY(glBindSampler),
    FUHGL_ENTRY(glSamplerParameteri), FUHGL_ENTRY(glSamplerParameterf),
    FUHGL_ENTRY(glTexStorage2D), FUHGL_ENTRY(glTexStorage3D), FUHGL_ENTRY(glTexStorage2DMultisample),
    FUHGL_ENTRY(glRenderbufferStorageMultisample),
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

} // namespace fuhgl::gl
