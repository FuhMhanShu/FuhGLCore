#pragma once

/// @file ext/bindless.hpp
/// @brief GL_ARB_bindless_texture (F35): 64-bit texture handles that shaders sample
/// directly (`uniform uint64_t uTex;` + `sampler2D(uTex)` in GLSL with
/// `#extension GL_ARB_bindless_texture : require`), no texture-unit binding needed.
/// Entry points are resolved via gl::procAddress; on platforms without the extension
/// (e.g. macOS) supported() returns false and handles are 0.

#include "../loader.hpp"
#include "../program.hpp"
#include "../texture.hpp"
#include "../types.hpp"

namespace fuhgl::gl::ext {

/// @brief ARB_bindless_texture entry points (nullptr when the extension is missing).
struct BindlessApi {
    GLuint64 (*getTextureHandle)(GLuint texture) = nullptr;              // glGetTextureHandleARB
    void (*makeTextureHandleResident)(GLuint64 handle) = nullptr;        // glMakeTextureHandleResidentARB
    void (*makeTextureHandleNonResident)(GLuint64 handle) = nullptr;     // glMakeTextureHandleNonResidentARB
    GLboolean (*isTextureHandleResident)(GLuint64 handle) = nullptr;     // glIsTextureHandleResidentARB
    void (*uniformHandleui64)(GLint location, GLuint64 value) = nullptr; // glUniformHandleui64ARB
    void (*programUniformHandleui64)(GLuint program, GLint location, GLuint64 value) = nullptr;
    /// @brief Resolves all entry points via gl::procAddress; false when unsupported.
    bool load();
};

/// @brief Lazily resolved API table (call after GL initialization).
const BindlessApi& api();
/// @brief True when GL_ARB_bindless_texture is present and all entry points resolved.
bool supported();

/// @brief RAII bindless handle for one texture: the handle is made resident on attach()
/// and non-resident again on detach()/destruction. The texture object itself stays owned
/// by the caller. Not a GL object wrapper — just the 64-bit handle, so no PIMPL.
class BindlessTexture {
public:
    BindlessTexture() = default;
    ~BindlessTexture() { detach(); }

    BindlessTexture(const BindlessTexture&) = delete;
    BindlessTexture& operator=(const BindlessTexture&) = delete;
    BindlessTexture(BindlessTexture&& o) noexcept : handle_(o.handle_) { o.handle_ = 0; }
    BindlessTexture& operator=(BindlessTexture&& o) noexcept {
        if (this != &o) {
            detach();
            handle_ = o.handle_;
            o.handle_ = 0;
        }
        return *this;
    }

    /// @brief Creates the handle for tex and makes it resident. false when unsupported.
    bool attach(const Texture& tex);
    /// @brief Makes the handle non-resident and resets it to 0 (no-op when already 0).
    void detach();

    GLuint64 handle() const { return handle_; }
    bool isResident() const;

private:
    GLuint64 handle_ = 0;
};

/// @brief Sets a uint64 sampler uniform on the currently used program (glUniformHandleui64ARB).
void setUniformHandle(GLint location, GLuint64 handle);
/// @brief Looks up the uniform name and sets the handle (uses the program's own id when
/// glProgramUniformHandleui64ARB is available, else glUniformHandleui64ARB after use()).
void setUniformHandle(Program& prog, const char* name, GLuint64 handle);

} // namespace fuhgl::gl::ext
