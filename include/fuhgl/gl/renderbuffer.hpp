#pragma once

/// @file renderbuffer.hpp
/// @brief RAII wrapper around a GL renderbuffer object.

#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief Renderbuffer (usually used as depth/stencil attachment of a Framebuffer).
class Renderbuffer {
public:
    Renderbuffer();
    ~Renderbuffer();

    Renderbuffer(const Renderbuffer&) = delete;
    Renderbuffer& operator=(const Renderbuffer&) = delete;
    Renderbuffer(Renderbuffer&&) noexcept;
    Renderbuffer& operator=(Renderbuffer&&) noexcept;

    bool create();
    void bind() const;
    static void unbind();
    /// @brief Allocates the storage, e.g. storage(GL_DEPTH24_STENCIL8, w, h).
    void storage(gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height) const;
    /// @brief Allocates multisampled storage (GL 3.3+; no-op if unsupported).
    void storageMultisample(gl::GLsizei samples, gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height) const;

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
