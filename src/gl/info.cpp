#include "fuhgl/gl/info.hpp"

#include <cstdio>
#include <cstring>

namespace fuhgl::gl {
namespace {

GLint geti(GLenum pname) {
    GLint v = 0;
    if (functions().glGetIntegerv) functions().glGetIntegerv(pname, &v);
    return v;
}

GLint64 geti64(GLenum pname) {
    const auto& f = functions();
    if (f.glGetInteger64v) {
        GLint64 v = 0;
        f.glGetInteger64v(pname, &v);
        return v;
    }
    return static_cast<GLint64>(geti(pname)); // legacy drivers: 32-bit query
}

std::string getString(GLenum name) {
    if (functions().glGetString)
        if (const GLubyte* s = functions().glGetString(name))
            return reinterpret_cast<const char*>(s);
    return {};
}

} // namespace

bool DeviceInfo::hasExtension(const char* name) const {
    if (!name) return false;
    for (const std::string& e : extensions)
        if (e == name) return true;
    return false;
}

std::string DeviceInfo::report() const {
    std::string out;
    char buf[512];
    std::snprintf(buf, sizeof buf, "OpenGL %d.%d%s\n  Vendor:     %s\n  Renderer:   %s\n  GLSL:       %s\n  Version:    %s\n",
                  version.major, version.minor, features.coreProfile ? " (core profile)" : "",
                  vendor.c_str(), renderer.c_str(), glslVersion.c_str(), versionString.c_str());
    out += buf;
    std::snprintf(buf, sizeof buf, "  Extensions: %zu\n", extensions.size());
    out += buf;
    out += "  Features:  ";
    const char* feats[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    int nf = 0;
    if (features.dsa) feats[nf++] = "DSA 4.5";
    if (features.compute) feats[nf++] = "compute";
    if (features.debugOutput) feats[nf++] = "debug output";
    if (features.anisotropicFiltering) feats[nf++] = "anisotropic filtering";
    if (features.shaderStorage) feats[nf++] = "SSBO";
    if (nf == 0) out += "(none beyond baseline)";
    for (int i = 0; i < nf; ++i) {
        out += feats[i];
        if (i + 1 < nf) out += ", ";
    }
    out += "\n  Limits:\n";
    const auto line = [&](const char* label, long long v) {
        std::snprintf(buf, sizeof buf, "    %-32s %lld\n", label, v);
        out += buf;
    };
    line("max 2D texture size", limits.maxTextureSize2D);
    line("max 3D texture size", limits.maxTextureSize3D);
    line("max cube map texture size", limits.maxTextureSizeCube);
    line("max array texture layers", limits.maxArrayTextureLayers);
    line("max renderbuffer size", limits.maxRenderbufferSize);
    std::snprintf(buf, sizeof buf, "    %-32s %d x %d\n", "max viewport dims", limits.maxViewportWidth, limits.maxViewportHeight);
    out += buf;
    line("max vertex attribs", limits.maxVertexAttribs);
    line("max vertex uniform vectors", limits.maxVertexUniformVectors);
    line("max varying vectors", limits.maxVaryingVectors);
    line("max fragment uniform vectors", limits.maxFragmentUniformVectors);
    line("max texture image units (frag)", limits.maxTextureImageUnits);
    line("max vertex texture image units", limits.maxVertexTextureImageUnits);
    line("max combined texture image units", limits.maxCombinedTextureImageUnits);
    line("max draw buffers", limits.maxDrawBuffers);
    line("max color attachments", limits.maxColorAttachments);
    line("max MSAA samples", limits.maxSamples);
    line("max uniform buffer bindings", limits.maxUniformBufferBindings);
    line("max uniform buffer size [bytes]", static_cast<long long>(limits.maxUniformBufferSize));
    line("max SSBO bindings", limits.maxShaderStorageBufferBindings);
    line("max SSBO size [bytes]", static_cast<long long>(limits.maxShaderStorageBufferSize));
    line("max compute work group invocations", limits.maxComputeWorkGroupInvocations);
    std::snprintf(buf, sizeof buf, "    %-32s %d / %d / %d\n", "max compute work group count", limits.maxComputeWorkGroupCount[0],
                  limits.maxComputeWorkGroupCount[1], limits.maxComputeWorkGroupCount[2]);
    out += buf;
    std::snprintf(buf, sizeof buf, "    %-32s %d / %d / %d\n", "max compute work group size", limits.maxComputeWorkGroupSize[0],
                  limits.maxComputeWorkGroupSize[1], limits.maxComputeWorkGroupSize[2]);
    out += buf;
    line("max elements vertices", limits.maxElementsVertices);
    line("max elements indices", limits.maxElementsIndices);
    line("max vertex output components", limits.maxVertexOutputComponents);
    line("max fragment input components", limits.maxFragmentInputComponents);
    line("max texture anisotropy", limits.maxAnisotropy);
    return out;
}

DeviceInfo queryDeviceInfo() {
    DeviceInfo info;
    if (!isInitialized()) return info;

    info.vendor = getString(GL_VENDOR);
    info.renderer = getString(GL_RENDERER);
    info.versionString = getString(GL_VERSION);
    info.glslVersion = getString(GL_SHADING_LANGUAGE_VERSION);

    // Prefer the numeric query (GL >= 3.0); fall back to parsing the string.
    // On old contexts GL_MAJOR_VERSION is an invalid enum: values stay 0 and
    // the error flag is drained below.
    GLint maj = 0, min = 0;
    if (functions().glGetIntegerv) {
        functions().glGetIntegerv(GL_MAJOR_VERSION, &maj);
        functions().glGetIntegerv(GL_MINOR_VERSION, &min);
    }
    if (maj > 0)
        info.version = Version{maj, min};
    else
        info.version = parseVersion(info.versionString.c_str());
    if (functions().glGetError)
        while (functions().glGetError() != GL_NO_ERROR) {}

    const GLint profileMask = geti(GL_CONTEXT_PROFILE_MASK);
    info.features.coreProfile = (profileMask & GL_CONTEXT_CORE_PROFILE_BIT) != 0;

    info.extensions = extensions();
    const auto hasExt = [&](const char* n) { return info.hasExtension(n); };
    info.features.dsa = hasDsa();
    info.features.compute = info.version.atLeast(4, 3) || hasExt("GL_ARB_compute_shader");
    info.features.debugOutput = info.version.atLeast(4, 3) || hasExt("GL_KHR_debug");
    info.features.anisotropicFiltering = hasExt("GL_EXT_texture_filter_anisotropic");
    info.features.shaderStorage = info.version.atLeast(4, 3) || hasExt("GL_ARB_shader_storage_buffer_object");

    Limits& l = info.limits;
    l.maxTextureSize2D = geti(GL_MAX_TEXTURE_SIZE);
    l.maxTextureSize3D = geti(GL_MAX_3D_TEXTURE_SIZE);
    l.maxTextureSizeCube = geti(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
    l.maxArrayTextureLayers = geti(GL_MAX_ARRAY_TEXTURE_LAYERS);
    l.maxRenderbufferSize = geti(GL_MAX_RENDERBUFFER_SIZE);
    if (functions().glGetIntegerv) {
        GLint vp[2] = {0, 0};
        functions().glGetIntegerv(GL_MAX_VIEWPORT_DIMS, vp);
        l.maxViewportWidth = vp[0];
        l.maxViewportHeight = vp[1];
        functions().glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_COUNT, l.maxComputeWorkGroupCount);
        functions().glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_SIZE, l.maxComputeWorkGroupSize);
    }
    l.maxVertexAttribs = geti(GL_MAX_VERTEX_ATTRIBS);
    l.maxVertexUniformVectors = geti(GL_MAX_VERTEX_UNIFORM_VECTORS);
    l.maxVaryingVectors = geti(GL_MAX_VARYING_VECTORS);
    l.maxFragmentUniformVectors = geti(GL_MAX_FRAGMENT_UNIFORM_VECTORS);
    l.maxTextureImageUnits = geti(GL_MAX_TEXTURE_IMAGE_UNITS);
    l.maxVertexTextureImageUnits = geti(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    l.maxCombinedTextureImageUnits = geti(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    l.maxDrawBuffers = geti(GL_MAX_DRAW_BUFFERS);
    l.maxColorAttachments = geti(GL_MAX_COLOR_ATTACHMENTS);
    l.maxSamples = geti(GL_MAX_SAMPLES);
    l.maxUniformBufferBindings = geti(GL_MAX_UNIFORM_BUFFER_BINDINGS);
    l.maxUniformBufferSize = geti64(GL_MAX_UNIFORM_BUFFER_SIZE);
    l.maxShaderStorageBufferBindings = geti(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    l.maxShaderStorageBufferSize = geti64(GL_MAX_SHADER_STORAGE_BUFFER_SIZE);
    l.maxComputeWorkGroupInvocations = geti(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
    l.maxElementsVertices = geti(GL_MAX_ELEMENTS_VERTICES);
    l.maxElementsIndices = geti(GL_MAX_ELEMENTS_INDICES);
    l.maxVertexOutputComponents = geti(GL_MAX_VERTEX_OUTPUT_COMPONENTS);
    l.maxFragmentInputComponents = geti(GL_MAX_FRAGMENT_INPUT_COMPONENTS);
    l.maxAnisotropy = info.features.anisotropicFiltering ? geti(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT) : 0;
    return info;
}

} // namespace fuhgl::gl
