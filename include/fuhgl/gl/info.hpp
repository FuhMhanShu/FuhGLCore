#pragma once

/// @file info.hpp
/// @brief Device info & capability queries for the current GL context:
/// GPU strings (vendor/renderer/version/GLSL), parsed version, profile flag,
/// extension list, a curated set of implementation limits and feature flags.
/// Pure value types (no GL handles), so no PIMPL — like the math module.

#include "loader.hpp"
#include "types.hpp"

#include <string>
#include <vector>

namespace fuhgl::gl {

/// @brief Selected implementation limits (glGetIntegerv / glGetInteger64v).
/// 64-bit sizes are in bytes; 0 means "query unavailable" (e.g. feature missing).
struct Limits {
    GLint maxTextureSize2D = 0;                    ///< GL_MAX_TEXTURE_SIZE
    GLint maxTextureSize3D = 0;                    ///< GL_MAX_3D_TEXTURE_SIZE
    GLint maxTextureSizeCube = 0;                  ///< GL_MAX_CUBE_MAP_TEXTURE_SIZE
    GLint maxArrayTextureLayers = 0;               ///< GL_MAX_ARRAY_TEXTURE_LAYERS
    GLint maxRenderbufferSize = 0;                 ///< GL_MAX_RENDERBUFFER_SIZE
    GLint maxViewportWidth = 0, maxViewportHeight = 0; ///< GL_MAX_VIEWPORT_DIMS
    GLint maxVertexAttribs = 0;                    ///< GL_MAX_VERTEX_ATTRIBS
    GLint maxVertexUniformVectors = 0;             ///< GL_MAX_VERTEX_UNIFORM_VECTORS
    GLint maxVaryingVectors = 0;                   ///< GL_MAX_VARYING_VECTORS
    GLint maxFragmentUniformVectors = 0;           ///< GL_MAX_FRAGMENT_UNIFORM_VECTORS
    GLint maxTextureImageUnits = 0;                ///< GL_MAX_TEXTURE_IMAGE_UNITS (fragment)
    GLint maxVertexTextureImageUnits = 0;          ///< GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS
    GLint maxCombinedTextureImageUnits = 0;        ///< GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
    GLint maxDrawBuffers = 0;                      ///< GL_MAX_DRAW_BUFFERS (MRT)
    GLint maxColorAttachments = 0;                 ///< GL_MAX_COLOR_ATTACHMENTS
    GLint maxSamples = 0;                          ///< GL_MAX_SAMPLES (MSAA)
    GLint maxUniformBufferBindings = 0;            ///< GL_MAX_UNIFORM_BUFFER_BINDINGS
    GLint64 maxUniformBufferSize = 0;              ///< GL_MAX_UNIFORM_BUFFER_SIZE, bytes
    GLint maxShaderStorageBufferBindings = 0;      ///< GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS
    GLint64 maxShaderStorageBufferSize = 0;        ///< GL_MAX_SHADER_STORAGE_BUFFER_SIZE, bytes
    GLint maxComputeWorkGroupInvocations = 0;      ///< GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS
    GLint maxComputeWorkGroupCount[3] = {0, 0, 0}; ///< GL_MAX_COMPUTE_WORK_GROUP_COUNT (x,y,z)
    GLint maxComputeWorkGroupSize[3] = {0, 0, 0};  ///< GL_MAX_COMPUTE_WORK_GROUP_SIZE (x,y,z)
    GLint maxElementsVertices = 0;                 ///< GL_MAX_ELEMENTS_VERTICES
    GLint maxElementsIndices = 0;                  ///< GL_MAX_ELEMENTS_INDICES
    GLint maxVertexOutputComponents = 0;           ///< GL_MAX_VERTEX_OUTPUT_COMPONENTS
    GLint maxFragmentInputComponents = 0;          ///< GL_MAX_FRAGMENT_INPUT_COMPONENTS
    GLint maxAnisotropy = 0;                       ///< GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT (0 = no EXT)
};

/// @brief High-level feature flags derived from version + extension list.
struct Features {
    bool coreProfile = false;          ///< GL_CONTEXT_CORE_PROFILE_BIT set
    bool dsa = false;                  ///< GL 4.5 DSA table available (gl::hasDsa())
    bool compute = false;              ///< GL >= 4.3 or GL_ARB_compute_shader
    bool debugOutput = false;          ///< GL >= 4.3 or GL_KHR_debug
    bool anisotropicFiltering = false; ///< GL_EXT_texture_filter_anisotropic
    bool shaderStorage = false;        ///< GL >= 4.3 or GL_ARB_shader_storage_buffer_object
    bool textureArrays = false;        ///< GL >= 3.0 (sampler2DArray, CSM depth arrays)
    bool bindlessTextures = false;     ///< GL_ARB_bindless_texture (F35)
};

/// @brief Everything queryable about the current context / GPU. Value type.
struct DeviceInfo {
    std::string vendor;        ///< GL_VENDOR
    std::string renderer;      ///< GL_RENDERER
    std::string versionString; ///< GL_VERSION, raw
    std::string glslVersion;   ///< GL_SHADING_LANGUAGE_VERSION
    Version version{};         ///< parsed GL version
    std::vector<std::string> extensions; ///< full extension list
    Limits limits;
    Features features;

    /// @brief True if the extension list contains exactly this name.
    bool hasExtension(const char* name) const;

    /// @brief Human-readable multi-line report (console output, logs, examples).
    std::string report() const;
};

/// @brief Query all of the above. Requires gl::init() + a current context;
/// returns an empty DeviceInfo when GL is not initialized.
DeviceInfo queryDeviceInfo();

} // namespace fuhgl::gl
