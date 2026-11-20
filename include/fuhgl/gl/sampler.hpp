#pragma once

/// @file sampler.hpp
/// @brief RAII wrapper around a GL sampler object (GL 3.3).

#include "types.hpp"

#include <memory>

namespace fuhgl {

/// @brief Sampler object: bind to a texture unit with bind(unit), then textures on
/// that unit use its filtering/wrapping parameters.
class Sampler {
public:
    Sampler();
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&&) noexcept;
    Sampler& operator=(Sampler&&) noexcept;

    bool create();
    /// @brief Binds this sampler to a texture unit (replaces glActiveTexture+glBindTexture state juggling).
    void bind(gl::GLuint unit) const;
    static void unbind(gl::GLuint unit);

    void setParameteri(gl::GLenum pname, gl::GLint param) const;
    void setParameterf(gl::GLenum pname, gl::GLfloat param) const;
    void setBorderColor(float r, float g, float b, float a) const;

    gl::GLuint id() const;
    explicit operator bool() const { return id() != 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fuhgl
