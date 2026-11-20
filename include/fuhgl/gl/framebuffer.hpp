#pragma once

/// @file framebuffer.hpp
/// @brief RAII wrapper around a GL framebuffer object.

#include "renderbuffer.hpp"
#include "texture.hpp"
#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief Framebuffer object: attach textures/renderbuffers, check completeness, bind.
class Framebuffer {
public:
    /// @brief Framebuffer binding target.
    enum class Target : gl::GLenum {
        Framebuffer = gl::GL_FRAMEBUFFER,
        Read = gl::GL_READ_FRAMEBUFFER,
        Draw = gl::GL_DRAW_FRAMEBUFFER,
    };
    /// @brief Attachment point.
    enum class Attachment : gl::GLenum {
        Color0 = gl::GL_COLOR_ATTACHMENT0,
        Depth = gl::GL_DEPTH_ATTACHMENT,
        Stencil = gl::GL_STENCIL_ATTACHMENT,
        DepthStencil = gl::GL_DEPTH_STENCIL_ATTACHMENT,
    };

    Framebuffer();
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&&) noexcept;
    Framebuffer& operator=(Framebuffer&&) noexcept;

    bool create();
    void bind(Target target = Target::Framebuffer) const;
    static void unbind(Target target = Target::Framebuffer);

    void attachTexture2D(Attachment attachment, const Texture& texture, gl::GLint level = 0) const;
    /// @brief Attaches to GL_COLOR_ATTACHMENT0 + index (for MRT, e.g. deferred G-buffer).
    void attachTexture2D(unsigned int colorIndex, const Texture& texture, gl::GLint level = 0) const;
    void attachRenderbuffer(Attachment attachment, const Renderbuffer& rb) const;
    /// @brief GL_FRAMEBUFFER_COMPLETE if complete, otherwise the specific status.
    gl::GLenum status() const;
    bool isComplete() const { return status() == gl::GL_FRAMEBUFFER_COMPLETE; }

    /// @brief Blits (copies/resolves) a region of this framebuffer into dst — used to
    /// resolve multisampled renderbuffers/textures.
    void blitTo(Framebuffer& dst, gl::GLint srcW, gl::GLint srcH, gl::GLint dstW, gl::GLint dstH,
                gl::GLbitfield mask, bool linearFilter = false) const;
    /// @brief Blits src into the default framebuffer (the screen/window).
    static void blitToDefault(const Framebuffer& src, gl::GLint dstW, gl::GLint dstH,
                              gl::GLbitfield mask = gl::GL_COLOR_BUFFER_BIT, bool linearFilter = false);

    /// @brief Records the target size (used by blitToDefault; pipelines set it on create/resize).
    void setSize(int w, int h);
    int width() const;
    int height() const;

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
